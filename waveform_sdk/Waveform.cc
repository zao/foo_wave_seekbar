//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Waveform.h"
#include "WaveformImpl.h"
#include "Downmix.h"

#include <list>

namespace wave
{
	service_ptr_t<waveform> downmix_waveform(service_ptr_t<waveform> const& w)
	{
		typedef pfc::list_t<float> t_channel;
		typedef pfc::list_t<float> t_frame;

		auto channel_count = w->get_channel_count();

		service_ptr_t<waveform_impl> ret = new service_impl_t<waveform_impl>;
		ret->channel_map = audio_chunk::channel_config_mono;

		std::list<pfc::string> field_names;
		field_names.push_back("minimum");
		field_names.push_back("maximum");
		field_names.push_back("rms");

		for (auto I = field_names.begin(); I != field_names.end(); ++I)
		{
			auto name = *I;

			pfc::list_t<float> frame;
			frame.set_count(channel_count);

			pfc::list_t<t_channel> channels;
			channels.set_count(channel_count);

			t_channel mix;
			mix.set_count(2048);

			for (size_t channel_idx = 0; channel_idx < channel_count; ++channel_idx)
			{
				w->get_field(name, channel_idx, channels[channel_idx]);
			}

			for (size_t sample_index = 0; sample_index < 2048; ++sample_index)
			{
				for (size_t channel_idx = 0; channel_idx < channel_count; ++channel_idx)
				{
					frame[channel_idx] = channels[channel_idx][sample_index];
				}
				mix[sample_index] = downmix(frame);
			}
			ret->fields[name].add_item(mix);
		}
		return ret;
	}
}
