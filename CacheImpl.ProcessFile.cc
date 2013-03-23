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
	class span
	{
		unsigned const channels;
		t_int64 frames, chunk_size;
		pfc::list_t<float> min, max, rms; // list of channels

	public:
		explicit span(t_int64 chunk_size, unsigned channels)
			: channels(channels), frames(0), chunk_size(chunk_size)
		{
			min.add_items_repeat( 9001.0f, channels);
			max.add_items_repeat(-9001.0f, channels);
			rms.add_items_repeat(    0.0f, channels);
		}

		void add(audio_sample* frame)
		{
			++frames;
			for (size_t c = 0; c < channels; ++c)
			{
				min[c] = std::min(frame[c], min[c]);
				max[c] = std::max(frame[c], max[c]);
				rms[c] += frame[c] * frame[c];
			}
		}

		void add(audio_sample* frame, t_int64 count)
		{
			frames += count;
			while (count--)
			{
				for (size_t c = 0; c < channels; ++c)
				{
					min[c] = std::min(frame[c], min[c]);
					max[c] = std::max(frame[c], max[c]);
					rms[c] += frame[c] * frame[c];
				}
				frame += channels;
			}
		}

		template <typename List>
		void resolve(List& min_result, List& max_result, List& rms_result) const
		{
			min_result = min;
			max_result = max;
			rms_result = rms;
			for (size_t c = 0; c < channels; ++c)
			{
				rms_result[c] = sqrt(rms_result[c] / frames);
			}
		}

		t_int64 frames_remaining() const
		{
			return chunk_size - frames;
		}
	};

	template <typename C1, typename C2>
	void transpose(C1& out, C2 const& in)
	{
		int in_rows = in.get_size(), in_cols = in[0].get_size();
		int out_rows = in_cols, out_cols = in_rows;

		out.set_size(out_rows);
		for (int out_row = 0; out_row < out_rows; ++out_row)
		{
			out[out_row].set_size(out_cols);
			for (int out_col = 0; out_col < out_cols; ++out_col)
			{
				out[out_row][out_col] = in[out_col][out_row];
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

		if (regex_match(loc.get_path(), boost::regex("(random|record):.*", boost::regex::perl | boost::regex::icase)) ||
			regex_match(loc.get_path(), boost::regex("(http|https|mms|lastfm|foo_lastfm_radio|tone)://.*", boost::regex::perl | boost::regex::icase)) ||
			regex_match(loc.get_path(), boost::regex("(cdda)://.*", boost::regex::perl | boost::regex::icase)) && !user_requested)
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
		} timer;

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
				file_info_impl info;
				decoder->initialize(subsong, input_flag_simpledecode, abort_cb);
				if (!decoder->can_seek())
					return out;
				decoder->get_info(subsong, info, abort_cb);

				t_int64 sample_rate = info.info_get_int("samplerate");
				{
					double foo;
					if (decoder->get_dynamic_info(info, foo))
					{
						auto dynamic_rate = info.info_get_int("samplerate");
						sample_rate = dynamic_rate ? dynamic_rate : sample_rate;
					}
				}
				t_int64 sample_count = info.info_get_length_samples();
				t_int64 chunk_size = sample_count / 2047;

				// around a month ought to be enough for anyone
				if (sample_count <= 0 || sample_count > sample_rate * 60 * 60 * 24 * 31)
					return out;

				pfc::list_t<pfc::list_t<float>> minimum, maximum, rms;

				audio_chunk_impl chunk;

				t_int64 sample_index = 0;
				t_int32 out_index = 0;

				t_int64 processed_samples = 0;

				unsigned channel_map = audio_chunk::channel_config_mono;
				boost::optional<unsigned> track_channel_count;
				scoped_ptr<span> current_span;

				audio_source source(abort_cb, decoder, sample_count);
				bool done = false;
				while (!done)
				{
					throw_if_aborting(abort_cb);
					source.render(chunk);

					if (minimum.get_count() == 0)
					{
						int nch = source.channel_count();
						pfc::list_t<float> ch_list;
						ch_list.set_size(2);
						minimum.add_items(pfc::list_single_ref_t<pfc::list_t<float>>(ch_list, 2048));
						maximum.add_items(pfc::list_single_ref_t<pfc::list_t<float>>(ch_list, 2048));
						rms.add_items(pfc::list_single_ref_t<pfc::list_t<float>>(ch_list, 2048));
					}

					audio_sample* data = chunk.get_data();
					channel_map = chunk.get_channel_config();
					if (processed_samples >= sample_count)
					{
						done = true;
						break;
					}
					t_int64 n = std::min<t_int64>(chunk.get_sample_count(), sample_count - processed_samples);
					for (t_int64 i = 0; i < n;)
					{						
						if (!current_span)
							current_span.reset(new span(chunk_size, source.channel_count()));
					
						audio_sample* frame = data + i * source.channel_count();
						auto wanted = std::min(current_span->frames_remaining(), n-i);
						current_span->add(frame, wanted);
						processed_samples += wanted;
						i += wanted;
						
						if (++sample_index == chunk_size)
						{
							sample_index = 0;
							if (out_index == 2047)
							{
								i = n; // hack-fix until proper rewrite, avoids eternal scan
								continue;
							}
							current_span->resolve(minimum[out_index], maximum[out_index], rms[out_index]);
							current_span.reset();
							++out_index;
						}
					}
				}

				if (current_span)
				{
					current_span->resolve(minimum[out_index], maximum[out_index], rms[out_index]);
				}
				
				if (minimum.get_size() == 0)
				{
					console::formatter() << "Wave cache: failed to render location " << loc;
					return out;
				}
				{
					if (should_downmix)
					{
						auto downmix_one = [](pfc::list_t<float>& l)
						{
							l[0] = downmix(l);
							l.set_size(1);
						};
						for (size_t i = 0; i < minimum.get_size(); ++i)
						{
							downmix_one(minimum[i]);
							downmix_one(maximum[i]);
							downmix_one(rms[i]);
						}
						channel_map = audio_chunk::channel_config_mono;
					}

					pfc::list_t<pfc::list_t<float>> tr_minimum, tr_maximum, tr_rms;
					throw_if_aborting(abort_cb);
					transpose(tr_minimum, minimum);
					throw_if_aborting(abort_cb);
					transpose(tr_maximum, maximum);
					throw_if_aborting(abort_cb);
					transpose(tr_rms, rms);

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
