//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include "waveform_sdk/WaveformImpl.h"
#include "waveform_sdk/Downmix.h"
#include "waveform_sdk/Optional.h"
#include "Helpers.h"
#include <regex>

// {1D06B944-342D-44FF-9566-AAC520F616C2}
static const GUID guid_downmix_in_analysis = { 0x1d06b944, 0x342d, 0x44ff, { 0x95, 0x66, 0xaa, 0xc5, 0x20, 0xf6, 0x16, 0xc2 } };

// {EC789B1B-23A0-45D7-AB7D-D40B4E3673E5}
static const GUID guid_analyse_tracks_outside_library = { 0xec789b1b, 0x23a0, 0x45d7, { 0xab, 0x7d, 0xd4, 0xb, 0x4e, 0x36, 0x73, 0xe5 } };

// {9752AFF1-DF5A-4F80-AB9E-B285AF48CB86}
static const GUID guid_report_incremental_results = { 0x9752aff1, 0xdf5a, 0x4f80, { 0xab, 0x9e, 0xb2, 0x85, 0xaf, 0x48, 0xcb, 0x86 } };


static advconfig_branch_factory g_seekbar_branch("Waveform Seekbar", guid_seekbar_branch, advconfig_entry::guid_branch_tools, 0.0);
static advconfig_checkbox_factory g_downmix_in_analysis("Store analysed tracks in mono", guid_downmix_in_analysis, guid_seekbar_branch, 0.0, false);
static advconfig_checkbox_factory g_analyse_tracks_outside_library("Analyse tracks not in the media library", guid_analyse_tracks_outside_library, guid_seekbar_branch, 0.0, true);
static advconfig_checkbox_factory g_report_incremental_results("Incremental update of waveforms being scanned", guid_report_incremental_results, guid_seekbar_branch, 0.0, false);

namespace wave
{
	t_int64 const bucket_count = 2048;

	void throw_if_aborting(abort_callback const& cb)
	{
		if (cb.is_aborting())
			throw foobar2000_io::exception_aborted();
	}

	struct duration_query
	{
		LARGE_INTEGER then;
		duration_query()
		{
			QueryPerformanceCounter(&then);
		}

		double get_elapsed() const
		{
			LARGE_INTEGER now, freq;
			QueryPerformanceCounter(&now);
			QueryPerformanceFrequency(&freq);
			return (double)(now.QuadPart - then.QuadPart) / (double)freq.QuadPart;
		}
	};

	struct scoped_timer
	{
		duration_query duration;
		scoped_timer()
		{
		}

		~scoped_timer()
		{
			double t = duration.get_elapsed();
			console::formatter() << "Scan took " << t << " seconds.";
		}
	};

	bool try_determine_song_parameters(service_ptr_t<input_decoder>& decoder, t_uint32 subsong,
		t_int64& sample_rate, t_int64& sample_count, abort_callback& abort_cb)
	{
		file_info_impl info;
		decoder->get_info(subsong, info, abort_cb);

		sample_rate = info.info_get_int("samplerate");
		{
			double foo;
			if (decoder->get_dynamic_info(info, foo))
			{
				auto dynamic_rate = info.info_get_int("samplerate");
				sample_rate = dynamic_rate ? dynamic_rate : sample_rate;
			}
		}
		sample_count = info.info_get_length_samples();
		return true;
	}

	template <typename C1, typename C2>
	void transpose(C1& out, C2 const& in, size_t width, int valid_input_rows)
	{
		int in_rows = in.get_size()/width, in_cols = width;
		int out_rows = in_cols, out_cols = in_rows;

		out.set_size(out_rows);
		for (int out_row = 0; out_row < out_rows; ++out_row)
		{
			out[out_row].set_size(out_cols);
			for (int out_col = 0; out_col < valid_input_rows; ++out_col)
			{
				out[out_row][out_col] = in[out_col*width + out_row];
			}
			for (int out_col = valid_input_rows; out_col < out_cols; ++out_col)
			{
				out[out_row][out_col] = 0.0f;
			}
		}
	}

	class channel_mismatch_exception : public std::exception
	{
	};

	class audio_source
	{
		abort_callback& abort_cb;
		service_ptr_t<input_decoder>& decoder;
		bool exhausted;
		int64_t sample_count;
		int64_t generated_samples;
		wave::optional<unsigned> track_channel_count;

		enum { SilenceChunkFrames = 16384 };

	public:
		audio_source(abort_callback& abort_cb, service_ptr_t<input_decoder>& decoder, int64_t sample_count)
			: abort_cb(abort_cb)
			, decoder(decoder)
			, exhausted(false)
			, sample_count(sample_count)
			, generated_samples(0)
		{
		}
		
		void render(audio_chunk& chunk)
		{
			if (exhausted || !decoder->run(chunk, abort_cb))
			{
				int64_t n = std::max(0LL, std::min<int64_t>(sample_count - generated_samples, SilenceChunkFrames));
				chunk.set_channels(*track_channel_count);
				chunk.set_silence((t_size)n);
				exhausted = true;
			}

			generated_samples += chunk.get_sample_count();

			unsigned channel_count = chunk.get_channels();
			if (!track_channel_count.valid())
				track_channel_count = channel_count;

			if (*track_channel_count != channel_count)
				throw channel_mismatch_exception();
		}

		unsigned channel_count() const { assert(track_channel_count.valid()); return *track_channel_count; }
	};

	class analysis_pass
	{
	protected:
		t_int64 sample_count;
		bool initialized;
		unsigned channel_count, channel_map;

	public:
		virtual ~analysis_pass() {}

		explicit analysis_pass(t_int64 sample_count)
			: sample_count(sample_count)
			, initialized(false)
			, channel_count(0)
			, channel_map(0)
		{}

		virtual void consume_input(audio_chunk const& chunk) = 0;
		virtual bool finished() const = 0;
	};

	struct waveform_builder : analysis_pass
	{
		// buckets with interleaved channels
		pfc::list_t<audio_sample> minimum, maximum, rms;
		unsigned bucket;
		t_int64 bucket_begins;
		t_int64 samples_processed;
		bool should_downmix;
		abort_callback& abort_cb;

		waveform_impl incremental_result;
		std::shared_ptr<cache_impl::incremental_result_sink> incremental_output;

		duration_query dur;
		double last_update;
		unsigned last_update_bucket;
		double const update_interval;

		waveform_builder(t_int64 sample_count, bool should_downmix, abort_callback& abort_cb,
			std::shared_ptr<cache_impl::incremental_result_sink> incremental_output)
			: analysis_pass(sample_count)
			, bucket(0)
			, bucket_begins(0)
			, samples_processed(0)
			, should_downmix(should_downmix)
			, abort_cb(abort_cb)
			, incremental_output(incremental_output)
			, last_update(0.0)
			, last_update_bucket(~0)
			, update_interval(0.5f)
		{}

		bool uninitialized() const
		{
			return !initialized;
		}

		bool valid_bucket() const
		{
			return bucket < bucket_count;
		}

		virtual bool finished() const override
		{
			return !valid_bucket();
		}

		t_int64 samples_remaining() const
		{
			return sample_count - samples_processed;
		}

		t_int64 bucket_ends() const
		{
			return ((bucket+1) * sample_count) / bucket_count;
		}

		t_int64 chunk_size() const
		{
			return bucket_ends() - bucket_begins;
		}

		bool bucket_boundary() const
		{
			return samples_processed == bucket_ends();
		}

		void initialize(unsigned channel_count, unsigned channel_map)
		{
			this->channel_count = channel_count;
			this->channel_map = channel_map;
			t_int32 const entry_count = channel_count*bucket_count;
			minimum.add_items_repeat(FLT_MAX, entry_count);
			maximum.add_items_repeat(-FLT_MAX, entry_count);
			rms.add_items_repeat(0.0f, entry_count);
			initialized = true;
		}

		virtual void consume_input(audio_chunk const& chunk) override
		{
			t_int64 n = std::min(samples_remaining(), (t_int64)chunk.get_sample_count());
			audio_sample const* data = chunk.get_data();
			for (t_int64 i = 0; i < n;)
			{
				t_int64 const to_process = std::min(bucket_ends() - samples_processed, n - i);
				process(data + i*channel_count, to_process);
				i += to_process;
				if (bucket_boundary())
				{
					finalize_bucket(to_process);
					double now = dur.get_elapsed();
					if (g_report_incremental_results.get() &&
						incremental_output &&
						last_update + update_interval <= now &&
						last_update_bucket != bucket)
					{
						last_update += update_interval;
						auto intermediary = finalize_waveform();
						(*incremental_output)(intermediary, bucket);
					}
				}
			}
		}

		void process(audio_sample const* data, t_int64 frames)
		{
			for (unsigned k = 0; k < frames * channel_count; ++k)
			{
				auto const target_offset = bucket*channel_count + (k%channel_count);
				audio_sample& min = minimum[target_offset];
				audio_sample& max = maximum[target_offset];
				audio_sample sample = *data++;
				min = std::min(min, sample);
				max = std::max(max, sample);
				rms[target_offset] += sample * sample;
			}
			samples_processed += frames;
		}

		void finalize_bucket(t_int64 last_part_size)
		{
			for (unsigned ch = 0; ch < channel_count; ++ch)
			{
				auto const target_offset = bucket*channel_count + ch;
				if (last_part_size == 0)
				{
					minimum[target_offset] = maximum[target_offset] = 0.0f;
				}
				rms[target_offset] = sqrt(rms[target_offset] / chunk_size());
			}
			t_int64 old_end = bucket_ends();
			++bucket;
			bucket_begins = old_end;
		}

		ref_ptr<waveform> finalize_waveform() const
		{
			auto channel_count = this->channel_count;
			auto channel_map = this->channel_map;
			auto minimum = this->minimum;
			auto maximum = this->maximum;
			auto rms = this->rms;
			if (should_downmix)
			{
				auto downmix_one = [this](audio_sample const* l) -> audio_sample
				{
					pfc::list_t<audio_sample> frame;
					frame.add_items_fromptr(l, this->channel_count);
					return downmix(frame);
				};
				for (size_t i = 0; i < bucket_count; ++i)
				{
					auto off = i*channel_count;
					minimum[i] = downmix_one(minimum.get_ptr()+off);
					maximum[i] = downmix_one(maximum.get_ptr()+off);
					rms[i]     = downmix_one(rms.get_ptr()+off);
				}
				channel_count = 1;
				channel_map = audio_chunk::channel_config_mono;
				auto new_list_size = (t_size)(bucket_count * channel_count);
				minimum.set_count(new_list_size);
				maximum.set_count(new_list_size);
				rms.set_count(new_list_size);
			}

			// one inner list per channel
			pfc::list_t<pfc::list_t<float>> tr_minimum, tr_maximum, tr_rms;
			{
				pfc::list_t<float> one_channel;
				one_channel.set_size(bucket_count);
				tr_minimum.add_items_repeat(one_channel, channel_count);
				tr_maximum.add_items_repeat(one_channel, channel_count);
				tr_rms.add_items_repeat(one_channel, channel_count);
			}

			throw_if_aborting(abort_cb);
			transpose(tr_minimum, minimum, channel_count, bucket);
			throw_if_aborting(abort_cb);
			transpose(tr_maximum, maximum, channel_count, bucket);
			throw_if_aborting(abort_cb);
			transpose(tr_rms, rms, channel_count, bucket);

			ref_ptr<waveform_impl> ret(new waveform_impl);
			ret->fields.set("minimum", tr_minimum);
			ret->fields.set("maximum", tr_maximum);
			ret->fields.set("rms", tr_rms);
			ret->channel_map = channel_map;

			return ret;
		}
	};

	bool is_of_forbidden_protocol(playable_location const& loc)
	{
		auto match_pi = [&](char const* pat){ return std::regex_match(loc.get_path(), std::regex(pat, std::regex_constants::icase)); };
		return match_pi("(random|record):.*") || match_pi("(http|https|mms|lastfm|foo_lastfm_radio|tone)://.*") || match_pi("(cdda)://.*");
	}

	struct process_state {
		size_t buckets_filled;
		ref_ptr<waveform> wf;
		service_ptr_t<input_decoder> decoder;
		abort_callback* abort_cb;

		std::unique_ptr<waveform_builder> builder;
		std::unique_ptr<audio_source> source;
	};

	void destroy_process_state(process_state* state) {
		delete state;
	}

	process_result::type cache_impl::process_file(service_ptr_t<waveform_query> q, std::shared_ptr<process_state>& state)
	{
		playable_location_impl loc = q->get_location();
		try {
			if (!state) {
				bool user_requested = q->get_forced() == waveform_query::forced_query;
				std::shared_ptr<incremental_result_sink> incremental_output = std::shared_ptr<incremental_result_sink>();

				if (is_of_forbidden_protocol(loc) && !user_requested)
				{
					console::formatter() << "Wave cache: skipping location " << loc;
					return process_result::elided;
				}

				{
					boost::lock_guard<boost::mutex> lk(cache_mutex);
					if (!store || flush_callback.is_aborting())
					{
						return process_result::aborted;
					}
					if (!user_requested && store->has(loc))
					{
						console::formatter() << "Wave cache: redundant request for " << loc;
						if (store->has(loc)) {
							return process_result::elided;
						}
					}
				}

				// Test whether tracks are in the Media Library or not
				if (!g_analyse_tracks_outside_library.get())
				{
					boost::promise<bool> promise;

					in_main_thread([loc, &promise]()
					{
						static_api_ptr_t<library_manager> lib;
						static_api_ptr_t<metadb> mdb;
						metadb_handle_ptr m;
						mdb->handle_create(m, loc);
						promise.set_value(lib->is_item_in_library(m));
					});

					auto res = promise.get_future();
					while (!flush_callback.is_aborting())
					{
						auto rc = res.wait_for(boost::chrono::milliseconds(200));
						if (rc == boost::future_status::ready)
						{
							bool in_library = res.get();
							if (!in_library)
								return process_result::elided;
							break;
						}
					}
				}

				std::string location_string;
				{
					std::ostringstream oss;
					oss << "\"" << loc.get_path() << "\" / index: " << loc.get_subsong_index();
					location_string = oss.str();
				}

				state.reset(new process_state, destroy_process_state);
				state->buckets_filled = 0;
				state->abort_cb = &flush_callback;
				bool should_downmix = g_downmix_in_analysis.get();

				if (!input_entry::g_is_supported_path(loc.get_path()))
					return process_result::failed;

				input_entry::g_open_for_decoding(state->decoder, 0, loc.get_path(), *state->abort_cb);

				t_uint32 subsong = loc.get_subsong();
				{
					state->decoder->initialize(subsong, input_flag_simpledecode, *state->abort_cb);
					if (!state->decoder->can_seek())
						return process_result::failed;

					t_int64 sample_rate = 0;
					t_int64 sample_count = 0;
					if (!try_determine_song_parameters(state->decoder, subsong, sample_rate, sample_count, *state->abort_cb))
						return process_result::failed;

					// around a month ought to be enough for anyone
					if (sample_count <= 0 || sample_count > sample_rate * 60 * 60 * 24 * 31)
						return process_result::failed;

					state->builder.reset(new waveform_builder(sample_count, should_downmix, *state->abort_cb, incremental_output));
					state->source.reset(new audio_source(*state->abort_cb, state->decoder, sample_count));
				}
				return process_result::not_done;
			}
			else {
				audio_chunk_impl chunk;
				if (!state->builder->finished()) {
					throw_if_aborting(*state->abort_cb);
					state->source->render(chunk);
					if (state->builder->uninitialized())
					{
						state->builder->initialize(chunk.get_channels(), chunk.get_channel_config());
					}
					state->builder->consume_input(chunk);
					state->buckets_filled = state->builder->bucket;
					return process_result::not_done;
				}
				else {
					state->wf = state->builder->finalize_waveform();

					console::formatter() << "Wave cache: finished analysis of " << loc;
					boost::lock_guard<boost::mutex> lk(cache_mutex);
					open_store();
					if (store)
						store->put(state->wf, loc);
					else
						console::formatter() << "Wave cache: could not open backend database, losing new data for " << loc;
					return process_result::done;
				}
			}
		}
		catch (foobar2000_io::exception_aborted&)
		{
			// NOTE(zao): Abort state is detected in caller.
			return process_result::aborted;
		}
		catch (foobar2000_io::exception_io_not_found& e)
		{
			console::formatter() << "Wave cache: could not open/find " << loc << ", " << e.what();
		}
		catch (foobar2000_io::exception_io& ex)
		{
			console::formatter() << "Wave cache: generic IO exception (" << ex.what() <<") for " << loc;
		}
		catch (channel_mismatch_exception&)
		{
			console::formatter() << "Wave cache: track with mismatching channels, bailing out on " << loc;
		}
		catch (std::exception& ex)
		{
			console::formatter() << "Wave cache: generic exception (" << ex.what() <<") for " << loc;
		}
		return process_result::failed;
	}

	float cache_impl::render_progress(process_state* state) {
		return state->buckets_filled / (float)bucket_count;
	}

	ref_ptr<waveform> cache_impl::render_waveform(process_state* state) {
		return state->wf;
	}
}