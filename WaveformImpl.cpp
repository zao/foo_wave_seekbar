#include "PchSeekbar.h"
#include "WaveformImpl.h"
#include <cassert>

namespace wave
{
	bool waveform_impl::get_field(pfc::string const& what, unsigned channel, pfc::list_base_t<float>& out)
	{
		if (channel < minimum.get_size() && 0 == pfc::string::g_compare(what, "minimum"))
		{
			out = minimum[channel];
			return true;
		}
		if (channel < maximum.get_size() && 0 == pfc::string::g_compare(what, "maximum"))
		{
			out = maximum[channel];
			return true;
		}
		if (channel < rms.get_size() && 0 == pfc::string::g_compare(what, "rms"))
		{
			out = rms[channel];
			return true;
		}
		return false;
	}

	unsigned waveform_impl::get_channel_count() const
	{
		unsigned count = minimum.get_count();
		assert(count == maximum.get_count() && count == rms.get_count());
		return minimum.get_count();
	}

	unsigned waveform_impl::get_channel_map() const
	{
		return channel_map;
	}
}