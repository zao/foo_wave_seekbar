#include "PchSeekbar.h"
#include "CacheImpl.h"

namespace wave{
	void cache_impl::process_file(playable_location_impl loc, bool user_requested)
	{
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

		if (regex_match(loc.get_path(), boost::regex("(http|cdda|mms)://.*", boost::regex::perl | boost::regex::icase)))
		{
			console::formatter() << "Wave cache: skipping location " << loc;
			return;
		}

		{
			boost::mutex::scoped_lock sl(cache_mutex);
			if (!store || flush_callback.is_aborting())
			{
				job_flush_queue.push_back(make_job(loc, user_requested));
				return;
			}
			if (!user_requested && store->has(loc))
			{
				console::formatter() << "Wave cache: redundant request for " << loc;
				return;
			}
		}

		try
		{
			service_ptr_t<input_decoder> decoder;
			abort_callback& abort_cb = flush_callback;
			
			if (!input_entry::g_is_supported_path(loc.get_path()))
				return;

			input_entry::g_open_for_decoding(decoder, 0, loc.get_path(), abort_cb);

			t_uint32 subsong = loc.get_subsong();
			{
				file_info_impl info;
				decoder->initialize(subsong, input_flag_simpledecode, abort_cb);
				if (!decoder->can_seek())
					return;
				decoder->get_info(subsong, info, abort_cb);

				t_int64 sampleRate = info.info_get_int("samplerate");
				if (!sampleRate)
				{
					double foo;
					decoder->get_dynamic_info(info, foo);
				}
				t_int64 sampleCount = info.info_get_length_samples();
				t_int64 chunkSize = sampleCount / 2048;

				pfc::list_hybrid_t<float, 2048> minimum, maximum, rms;
				minimum.add_items_repeat(9001.0, 2048);
				maximum.add_items_repeat(-9001.0, 2048);
				rms.add_items_repeat(0.0, 2048);

				audio_chunk_impl chunk;

				const audio_sample sqrt_half = audio_sample(0.70710678118654752440084436210485);
				t_int64 sampleIndex = 0;
				t_int32 outIndex = 0;
				while (decoder->run(chunk, abort_cb))
				{
					audio_sample* data = chunk.get_data();
					unsigned channelCount = chunk.get_channels();
					for (t_size i = 0; i < chunk.get_sample_count(); ++i)
					{
						audio_sample* frame = data + i * channelCount;
						switch (channelCount)
						{
						case 8:
							frame[0] += frame[6] * sqrt_half;
							frame[1] += frame[7] * sqrt_half;
						case 6:
							frame[0] += frame[2] * sqrt_half + frame[4] * sqrt_half + frame[3];
							frame[1] += frame[2] * sqrt_half + frame[5] * sqrt_half + frame[3];
						case 2:
							frame[0] += frame[1];
							frame[0] /= 2.0;
							break;
						case 4:
							frame[0] += frame[1] + frame[2] + frame[3];
							frame[0] /= 4.0;
						}
						minimum[outIndex] = std::min(minimum[outIndex], frame[0]);
						maximum[outIndex] = std::max(maximum[outIndex], frame[0]);
						rms[outIndex] += frame[0] * frame[0];
						if (sampleIndex++ == chunkSize)
						{
							if (outIndex != 2047)
								++outIndex;
							sampleIndex = 0;
						}
					}
				}
				for (t_size i = 0; i < 2048; ++i)
				{
					rms[i] = sqrt(rms[i] / chunkSize);
				}

				service_ptr_t<waveform_impl> out = new service_impl_t<waveform_impl>;
				out->minimum.add_items(minimum);
				out->maximum.add_items(maximum);
				out->rms.add_items(rms);

				console::formatter() << "Wave cache: finished analysis of " << loc;
				boost::mutex::scoped_lock sl(cache_mutex);
				open_store();
				if (store)
					store->put(out, loc);
				else
					console::formatter() << "Wave cache: could not open backend database, losing new data for " << loc;
			}
		}
		catch (foobar2000_io::exception_aborted&)
		{
			job_flush_queue.push_back(make_job(loc, user_requested));
		}
		catch (foobar2000_io::exception_io_not_found&)
		{
			console::formatter() << "Wave cache: could not open/find " << loc;
		}
		catch (foobar2000_io::exception_io&)
		{
			console::formatter() << "Wave cache: generic IO exception on file " << loc;
		}
	}
}