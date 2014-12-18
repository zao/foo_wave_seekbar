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
	ref_ptr<waveform> downmix_waveform(ref_ptr<waveform> w, size_t target_channels)
	{
		typedef pfc::list_t<float> t_channel;
		typedef pfc::list_t<float> t_frame;

		auto channel_count = w->get_channel_count();

		ref_ptr<waveform_impl> ret(new waveform_impl);
		switch(target_channels)
		{
		case 1: ret->channel_map = audio_chunk::channel_config_mono; break;
		case 2: ret->channel_map = audio_chunk::channel_config_stereo; break;
		default: return make_placeholder_waveform();
		}

		std::list<char const*> field_names;
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

			pfc::list_t<t_channel> mix;
			for (size_t channel_idx = 0; channel_idx < target_channels; ++channel_idx)
			{
				t_channel buf;
				buf.set_count(2048);
				mix.add_item(buf);
			}

			for (size_t channel_idx = 0; channel_idx < channel_count; ++channel_idx)
			{
				w->get_field(name, channel_idx, list_array_sink<float>(channels[channel_idx]));
			}

			for (size_t sample_index = 0; sample_index < 2048; ++sample_index)
			{
				for (size_t channel_idx = 0; channel_idx < channel_count; ++channel_idx)
				{
					frame[channel_idx] = channels[channel_idx][sample_index];
				}
				switch (target_channels)
				{
				case 1: mix[0][sample_index] = downmix(frame); break;
				case 2:
					{
						auto pair = downmix_to_stereo(frame);
						mix[0][sample_index] = pair.first;
						mix[1][sample_index] = pair.second;
						break;
					}
				}
			}
			ret->fields[name].add_items(mix);
		}
		return ret;
	}

	struct waveform_placeholder : waveform
	{
		waveform_placeholder()
		{
			minimum.set_size(2048);
			maximum.set_size(2048);
			rms.set_size(2048);
			for (size_t i = 0; i < 2048; ++i)
			{
				minimum[i] = 0.0f;
				maximum[i] = 0.0f;
				rms[i] = 0.0f;
			}
		}

		virtual bool get_field(char const* what, unsigned index, array_sink<float> const& out)
		{
			if (index >= get_channel_count())
				return false;
			if (pfc::string::g_equals(what, "minimum"))
				return out.set(minimum.get_ptr(), minimum.get_count()), true;
			if (pfc::string::g_equals(what, "maximum"))
				return out.set(maximum.get_ptr(), maximum.get_count()), true;
			if (pfc::string::g_equals(what, "rms"))
				return out.set(rms.get_ptr(), rms.get_count()), true;
			return false;
		}

		virtual unsigned get_channel_count() const { return audio_chunk::defined_channel_count; }
		virtual unsigned get_channel_map() const { return (1 << audio_chunk::defined_channel_count) - 1; } // channel mask of bits 0 to 17 set.
		virtual ref_ptr<waveform> clone() const {
			waveform_placeholder* out = new waveform_placeholder;
			out->minimum = minimum;
			out->maximum = maximum;
			out->rms = rms;
			return ref_ptr<waveform>(out);
		}

	private:
		pfc::list_t<float> minimum, maximum, rms;
	};

	ref_ptr<waveform> make_placeholder_waveform()
	{
		return ref_ptr<waveform>(new waveform_placeholder);
	}
}