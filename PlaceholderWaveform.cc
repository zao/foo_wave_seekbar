#include "PchSeekbar.h"
#include "Waveform.h"

namespace wave
{
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

		virtual bool get_field(pfc::string const& what, unsigned index, pfc::list_base_t<float>& out)
		{
			if (index >= get_channel_count())
				return false;
			if (pfc::string::g_equals(what, "minimum"))
				return out = minimum, true;
			if (pfc::string::g_equals(what, "maximum"))
				return out = maximum, true;
			if (pfc::string::g_equals(what, "rms"))
				return out = rms, true;
			return false;
		}

		virtual unsigned get_channel_count() const { return audio_chunk::defined_channel_count; }
		virtual unsigned get_channel_map() const { return (1 << audio_chunk::defined_channel_count) - 1; } // channel mask of bits 0 to 17 set.

	private:
		pfc::list_hybrid_t<float, 2048> minimum, maximum, rms;
	};

	service_ptr_t<waveform> make_placeholder_waveform()
	{
		return new service_impl_t<waveform_placeholder>;
	}
}