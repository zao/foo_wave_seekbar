//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include "waveform_sdk/WaveformImpl.h"
#include "waveform_sdk/Downmix.h"
#include "Helpers.h"

#include <boost/regex.hpp>

// {1D06B944-342D-44FF-9566-AAC520F616C2}
static const GUID guid_downmix_in_analysis = { 0x1d06b944, 0x342d, 0x44ff, { 0x95, 0x66, 0xaa, 0xc5, 0x20, 0xf6, 0x16, 0xc2 } };

// {EC789B1B-23A0-45D7-AB7D-D40B4E3673E5}
static const GUID guid_analyse_tracks_outside_library = { 0xec789b1b, 0x23a0, 0x45d7, { 0xab, 0x7d, 0xd4, 0xb, 0x4e, 0x36, 0x73, 0xe5 } };

static advconfig_branch_factory g_seekbar_branch("Waveform Seekbar", guid_seekbar_branch, advconfig_entry::guid_branch_tools, 0.0);
static advconfig_checkbox_factory g_downmix_in_analysis("Store analysed tracks in mono", guid_downmix_in_analysis, guid_seekbar_branch, 0.0, false);
static advconfig_checkbox_factory g_analyse_tracks_outside_library("Analyse tracks not in the media library", guid_analyse_tracks_outside_library, guid_seekbar_branch, 0.0, true);

namespace wave
{
	struct scoped_timer
	{
		LARGE_INTEGER then;
		scoped_timer()
		{
			QueryPerformanceCounter(&then);
		}

		~scoped_timer()
		{
			LARGE_INTEGER now, freq;
			QueryPerformanceCounter(&now);
			QueryPerformanceFrequency(&freq);
			double t = (double)(now.QuadPart - then.QuadPart) / (double)freq.QuadPart;
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
	void transpose(C1& out, C2 const& in, size_t width)
	{
		int in_rows = in.get_size()/width, in_cols = width;
		int out_rows = in_cols, out_cols = in_rows;

		out.set_size(out_rows);
		for (int out_row = 0; out_row < out_rows; ++out_row)
		{
			out[out_row].set_size(out_cols);
			for (int out_col = 0; out_col < out_cols; ++out_col)
			{
				out[out_row][out_col] = in[out_col*width + out_row];
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
		optional<unsigned> track_channel_count;

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
			if (!track_channel_count)
				track_channel_count = channel_count;

			if (*track_channel_count != channel_count)
				throw channel_mismatch_exception();
		}

		unsigned channel_count() const { assert(track_channel_count); return *track_channel_count; }
	};

	void throw_if_aborting(abort_callback const& cb)
	{	
		if (cb.is_aborting())
			throw foobar2000_io::exception_aborted();
	}

	bool is_of_forbidden_protocol(playable_location const& loc)
	{
		auto match_pi = [&](char const* pat){ return regex_match(loc.get_path(), boost::regex(pat, boost::regex::perl | boost::regex::icase)); };
		return match_pi("(random|record):.*") || match_pi("(http|https|mms|lastfm|foo_lastfm_radio|tone)://.*") || match_pi("(cdda)://.*");
	}

	ref_ptr<waveform> cache_impl::process_file(playable_location_impl loc, bool user_requested)
	{
		ref_ptr<waveform> out;

		// Check for priority jobs.
		if (user_requested)
		{
			bool done = false;
			playable_location_impl prio_loc;
			while (true)
			{
				{
					boost::mutex::scoped_lock sl(important_mutex);
					if (important_queue.empty())
						break;
					prio_loc = important_queue.top();
					important_queue.pop();
				}
				process_file(prio_loc, false);
			}
		}

		if (is_of_forbidden_protocol(loc) && !user_requested)
		{
			console::formatter() << "Wave cache: skipping location " << loc;
			return out;
		}

		{
			boost::mutex::scoped_lock sl(cache_mutex);
			if (!store || flush_callback.is_aborting())
			{
				job_flush_queue.push_back(make_job(loc, user_requested));
				return out;
			}
			if (!user_requested && store->has(loc))
			{
				console::formatter() << "Wave cache: redundant request for " << loc;
				if (store->get(out, loc))
					return out;
			}
		}

		// Test whether tracks are in the Media Library or not
		if (!g_analyse_tracks_outside_library.get())
		{
			struct result
			{
				shared_ptr<boost::promise<bool>> p;
				boost::shared_future<bool> res;

				result()
					: p(new boost::promise<bool>), res(p->get_future())
				{}
			} res;

			in_main_thread([loc, res]()	
			{
				static_api_ptr_t<library_manager> lib;
				static_api_ptr_t<metadb> mdb;
				metadb_handle_ptr m;
				mdb->handle_create(m, loc);
				res.p->set_value(lib->is_item_in_library(m));
			});
			
			boost::shared_future<bool> f = res.res;
			while (!flush_callback.is_aborting())
			{
				if (f.timed_wait(boost::posix_time::milliseconds(200)))
				{
					if (!f.get())
						return out;
					break;
				}
			}
		}

		try
		{
			bool should_downmix = g_downmix_in_analysis.get();
			service_ptr_t<input_decoder> decoder;
			abort_callback& abort_cb = flush_callback;
			
			if (!input_entry::g_is_supported_path(loc.get_path()))
				return out;

			input_entry::g_open_for_decoding(decoder, 0, loc.get_path(), abort_cb);

			t_uint32 subsong = loc.get_subsong();
			{
				decoder->initialize(subsong, input_flag_simpledecode, abort_cb);
				if (!decoder->can_seek())
					return out;

				t_int64 sample_rate = 0;
				t_int64 sample_count = 0;
				if (!try_determine_song_parameters(decoder, subsong, sample_rate, sample_count, abort_cb))
					return out;

				// around a month ought to be enough for anyone
				if (sample_count <= 0 || sample_count > sample_rate * 60 * 60 * 24 * 31)
					return out;

				audio_chunk_impl chunk;
				
				t_int64 const bucket_count = 2048;
				unsigned bucket = 0;
				t_int64 bucket_begins = 0;
				t_int64 samples_processed = 0;

				// buckets with interleaved channels
				pfc::list_t<audio_sample> minimum, maximum, rms;

				audio_source source(abort_cb, decoder, sample_count);
				unsigned channel_count = 0;
				unsigned channel_map = 0;
				while (bucket < bucket_count)
				{
					throw_if_aborting(abort_cb);
					source.render(chunk);
					if (channel_count == 0)
					{
						channel_count = chunk.get_channels();
						channel_map = chunk.get_channel_config();
						t_int32 const entry_count = channel_count*bucket_count;
						minimum.add_items_repeat(FLT_MAX, entry_count); maximum.add_items_repeat(FLT_MIN, entry_count); rms.add_items_repeat(0.0f, entry_count);
					}

					t_int64 n = std::min(sample_count - samples_processed, (t_int64)chunk.get_sample_count());
					audio_sample const* data = chunk.get_data();
					for (t_int64 i = 0; i < n;)
					{
						t_int64 const bucket_ends = ((bucket+1) * sample_count) / bucket_count;
						t_int64 const chunk_size = bucket_ends - bucket_begins;
						t_int64 const to_process = std::min(bucket_ends - samples_processed, n - i);
						for (unsigned k = 0; k < to_process * channel_count; ++k)
						{
							auto const target_offset = bucket*channel_count + (k%channel_count);
							audio_sample& min = minimum[target_offset];
							audio_sample& max = maximum[target_offset];
							audio_sample sample = *data++;
							min = std::min(min, sample);
							max = std::max(max, sample);
							rms[target_offset] += sample * sample;
						}
						i += to_process;
						samples_processed += to_process;
						if (samples_processed == bucket_ends)
						{
							for (unsigned ch = 0; ch < channel_count; ++ch)
							{
								auto const target_offset = bucket*channel_count + ch;
								if (to_process == 0)
								{
									minimum[target_offset] = maximum[target_offset] = 0.0f;
								}
								rms[target_offset] = sqrt(rms[target_offset] / chunk_size);
							}
							++bucket;
							if (bucket == bucket_count)
							{
								break;
							}
						}
					}
				}

				{
					if (should_downmix)
					{
						auto downmix_one = [channel_count](audio_sample const* l) -> audio_sample
						{
							pfc::list_t<audio_sample> frame;
							frame.add_items_fromptr(l, channel_count);
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
					transpose(tr_minimum, minimum, channel_count);
					throw_if_aborting(abort_cb);
					transpose(tr_maximum, maximum, channel_count);
					throw_if_aborting(abort_cb);
					transpose(tr_rms, rms, channel_count);

					ref_ptr<waveform_impl> ret(new waveform_impl);
					ret->fields.set("minimum", tr_minimum);
					ret->fields.set("maximum", tr_maximum);
					ret->fields.set("rms", tr_rms);
					ret->channel_map = channel_map;

					out = ret;
				}

				console::formatter() << "Wave cache: finished analysis of " << loc;
				boost::mutex::scoped_lock sl(cache_mutex);
				open_store();
				if (store)
					store->put(out, loc);
				else
					console::formatter() << "Wave cache: could not open backend database, losing new data for " << loc;
				return out;
			}
		}
		catch (foobar2000_io::exception_aborted&)
		{
			boost::mutex::scoped_lock sl(cache_mutex);
			job_flush_queue.push_back(make_job(loc, user_requested));
		}
		catch (foobar2000_io::exception_io_not_found&)
		{
			console::formatter() << "Wave cache: could not open/find " << loc;
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
		return out;
	}
}
