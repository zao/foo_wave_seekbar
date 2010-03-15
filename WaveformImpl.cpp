#include "PchSeekbar.h"
#include "WaveformImpl.h"

namespace wave
{
	bool waveform_impl::get_field(pfc::string const& what, pfc::list_base_t<float>& out)
	{
		if (0 == pfc::string::g_compare(what, "minimum"))
		{
			out = minimum;
			return true;
		}
		if (0 == pfc::string::g_compare(what, "maximum"))
		{
			out = maximum;
			return true;
		}
		if (0 == pfc::string::g_compare(what, "rms"))
		{
			out = rms;
			return true;
		}
		return false;
	}
}