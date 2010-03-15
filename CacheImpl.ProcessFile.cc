#include "PchSeekbar.h"
#include "CacheImpl.h"

namespace wave
{
	const audio_sample sqrt_half = audio_sample(0.70710678118654752440084436210485);
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

		void resolve(float& min_result, float& max_result, float& rms_result) const
		{
			pfc::list_t<float> rms_tmp = rms;
			for (size_t c = 0; c < channels; ++c)
			{
				rms_tmp[c] = sqrt(rms_tmp[c] / samples);
			}
			min_result = downmix(min);
			max_result = downmix(max);
			rms_result = downmix(rms_tmp);
		}

	private:
		float downmix(pfc::list_t<float> const& frame) const
		{
			pfc::list_t<float> data = frame;
			switch (data.get_size())
			{
			case 8:
				data[0] += frame[6] * sqrt_half;
				data[1] += frame[7] * sqrt_half;
			case 6:
				data[0] += frame[2] * sqrt_half + frame[4] * sqrt_half + frame[3];
				data[1] += frame[2] * sqrt_half + frame[5] * sqrt_half + frame[3];
			case 2:
				data[0] += frame[1];
				data[0] /= 2.0;
				break;
			case 4:
				data[0] += frame[1] + frame[2] + frame[3];
				data[0] /= 4.0;
			}
			return data[0];
		}
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
			regex_match(loc.get_path(), boost::regex("(http|mms)://.*", boost::regex::perl | boost::regex::icase)) ||
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

		try
		{
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
				t_int64 chunk_size = sample_count / 2048;

				pfc::list_hybrid_t<float, 2048> minimum, maximum, rms;
				minimum.add_items_repeat(9001.0, 2048);
				maximum.add_items_repeat(-9001.0, 2048);
				rms.add_items_repeat(0.0, 2048);

				audio_chunk_impl chunk;

				t_int64 sample_index = 0;
				t_int32 out_index = 0;

				scoped_ptr<span> current_span;
				while (decoder->run(chunk, abort_cb))
				{
					unsigned channel_count = chunk.get_channels();

					audio_sample* data = chunk.get_data();
					for (t_size i = 0; i < chunk.get_sample_count(); ++i)
					{						
						if (!current_span)
							current_span.reset(new span(channel_count));
					
						audio_sample* frame = data + i * channel_count;
						current_span->add(frame);
						
						if (sample_index++ == chunk_size)
						{
							if (out_index != 2047)
								++out_index;
							current_span->resolve(minimum[out_index], maximum[out_index], rms[out_index]);
							current_span.reset();
							sample_index = 0;
						}
					}
				}

				{
					service_ptr_t<waveform_impl> ret = new service_impl_t<waveform_impl>;
					ret->minimum.add_items(minimum);
					ret->maximum.add_items(maximum);
					ret->rms.add_items(rms);
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
		catch (std::exception& ex)
		{
			console::formatter() << "Wave cache: generic exception (" << ex.what() <<") for " << loc;
		}
		return out;
	}
}