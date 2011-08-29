//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Waveform.h"
#include "WaveformImpl.h"
#include "Downmix.h"

#include <list>

#include <boost/foreach.hpp>

namespace wave
{
	namespace waveform
	{
		data* create(unsigned channel_map)
		{
			unsigned channel_count = audio_chunk::g_count_channels(channel_map);
			auto ret = (data*)malloc(sizeof(data) + 2048 * sizeof(float) * 3 * channel_count);
			ret->channel_count = channel_count;
			ret->channel_map = channel_map;
			return ret;
		}

		void destroy(data* p)
		{
			if (p)
				free(p);
		}

		float* get_field(data* p, unsigned channel_idx, field_tag tag)
		{
			auto channel_pitch = 2048;
			auto field_pitch = get_channel_count(p) * channel_pitch;
			auto ix = field_pitch * tag + channel_pitch * channel_idx;
			return p->storage + ix;
		}

		float const* get_field(data const* p, unsigned channel_idx, field_tag tag)
		{
			auto channel_pitch = 2048;
			auto field_pitch = get_channel_count(p) * channel_pitch;
			auto ix = field_pitch * tag + channel_pitch * channel_idx;
			return p->storage + ix;
		}

		unsigned get_channel_count(data const* p)
		{
			return p->channel_count;
		}

		unsigned get_channel_map(data const* p)
		{
			return p->channel_map;
		}

		data* downmix(data const* w)
		{
			typedef pfc::list_t<float> t_channel;

			auto channel_count = get_channel_count(w);

			data* ret = create(audio_chunk::channel_config_mono);

			std::list<field_tag> field_tags;
			field_tags.push_back(min_field);
			field_tags.push_back(max_field);
			field_tags.push_back(rms_field);

			BOOST_FOREACH(field_tag tag, field_tags)
			{
				pfc::list_t<float> frame;
				frame.set_count(channel_count);

				pfc::list_t<float const*> channels;
				channels.set_count(channel_count);

				float* mix = get_field(ret, 0, tag);

				for (size_t channel_idx = 0; channel_idx < channel_count; ++channel_idx)
				{
					channels[channel_idx] = get_field(w, channel_idx, tag);
				}

				for (size_t sample_index = 0; sample_index < 2048; ++sample_index)
				{
					for (size_t channel_idx = 0; channel_idx < channel_count; ++channel_idx)
					{
						frame[channel_idx] = channels[channel_idx][sample_index];
					}
					mix[sample_index] = ::downmix(frame);
				}
			}
			return ret;
		}

		data* make_placeholder()
		{
			return create((1 << audio_chunk::defined_channel_count) - 1);
		}
	}
}
