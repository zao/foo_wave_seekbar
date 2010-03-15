#pragma once
#include "Waveform.h"

namespace wave
{
	struct waveform_impl : waveform
	{
		virtual bool get_field(pfc::string const& what, pfc::list_base_t<float>& out) override;
		pfc::list_t<float> minimum, maximum, rms;
	};
}