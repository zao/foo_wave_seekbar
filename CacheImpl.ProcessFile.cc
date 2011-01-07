#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include "WaveformImpl.h"
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
		unsigned channels, samples;
		pfc::list_t<float> min, max, rms; // list of channels

	public:
		explicit span(unsigned channels)
			: channels(channels), samples(0)
		{
			min.add_items_repeat( 9001.0f, channels);
			max.add_items_repeat(-9001.0f, channels);
			rms.add_items_repeat(    0.0f, channels);
		}

		void add(audio_sample* frame)
		{
			for (size_t c = 0; c < channels; ++c)
			{
				min[c] = std::min(frame[c], min[c]);
				max[c] = std::max(frame[c], max[c]);
				rms[c] += frame[c] * frame[c];
			}
			++samples;
		}

		template <typename List>
		void resolve(List& min_result, List& max_result, List& rms_result) const
		{
			min_result = min;
			max_result = max;
			rms_result = rms;
			for (size_t c = 0; c < channels; ++c)
			{
				rms_result[c] = sqrt(rms_result[c] / samples);
			}
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

	service_ptr_t<waveform> cache_impl::process_file(playable_location_impl loc, bool user_requested)
	{
		service_ptr_t<waveform> out;
		// Check for priority jobs.
		if (user_requested)
		{
			bool done = false;
			playable_location_impl loc;
			while (true)
			{
				{
					boost::mutex::scoped_lock sl(important_mutex);
					if (important_queue.empty())
						break;
					loc = important_queue.top();
					important_queue.pop();
				}
				process_file(loc, false);
			}
		}

		if (regex_match(loc.get_path(), boost::regex("(random|record):.*", boost::regex::perl | boost::regex::icase)) ||
			regex_match(loc.get_path(), boost::regex("(http|https|mms|lastfm|foo_lastfm_radio)://.*", boost::regex::perl | boost::regex::icase)) ||
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
				store->get(out, loc);
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
				file_info_impl info;
				decoder->initialize(subsong, input_flag_simpledecode, abort_cb);
				if (!decoder->can_seek())
					return out;
				decoder->get_info(subsong, info, abort_cb);

				t_int64 sample_rate = info.info_get_int("samplerate");
				if (!sample_rate)
				{
					double foo;
					decoder->get_dynamic_info(info, foo);
				}
				t_int64 sample_count = info.info_get_length_samples();
				t_int64 chunk_size = sample_count / 2047;

				pfc::list_hybrid_t<pfc::list_t<float>, 2048> minimum, maximum, rms;
				minimum.add_items(pfc::list_single_ref_t<pfc::list_t<float>>(pfc::list_t<float>(), 2048));
				maximum.add_items(pfc::list_single_ref_t<pfc::list_t<float>>(pfc::list_t<float>(), 2048));
				rms.add_items(pfc::list_single_ref_t<pfc::list_t<float>>(pfc::list_t<float>(), 2048));

				audio_chunk_impl chunk;

				t_int64 sample_index = 0;
				t_int32 out_index = 0;

				t_int64 processed_samples = 0;

				unsigned channel_map = audio_chunk::channel_config_mono;
				boost::optional<unsigned> track_channel_count;
				scoped_ptr<span> current_span;

				bool done = false;
				while (!done)
				{
					if (!decoder->run(chunk, abort_cb))
					{
						chunk.set_silence((t_size)(sample_count - processed_samples));
						done = true;
					}
					unsigned channel_count = chunk.get_channels();
					if (!track_channel_count)
						track_channel_count = channel_count;

					if (*track_channel_count != channel_count)
						throw channel_mismatch_exception();

					audio_sample* data = chunk.get_data();
					channel_map = chunk.get_channel_config();
					for (t_size i = 0; i < chunk.get_sample_count(); ++i)
					{						
						if (!current_span)
							current_span.reset(new span(channel_count));
					
						audio_sample* frame = data + i * channel_count;
						current_span->add(frame);
						++processed_samples;
						
						if (++sample_index == chunk_size)
						{
							sample_index = 0;
							if (out_index == 2047)
								continue;
							current_span->resolve(minimum[out_index], maximum[out_index], rms[out_index]);
							current_span.reset();
							++out_index;
						}
						if (processed_samples >= sample_count)
						{
							done = true;
							break;
						}
					}
				}

				if (current_span)
				{
					current_span->resolve(minimum[out_index], maximum[out_index], rms[out_index]);
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

					pfc::list_t<pfc::list_hybrid_t<float, 2048>> tr_minimum, tr_maximum, tr_rms;
					transpose(tr_minimum, minimum);
					transpose(tr_maximum, maximum);
					transpose(tr_rms, rms);

					service_ptr_t<waveform_impl> ret = new service_impl_t<waveform_impl>;
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