#pragma once
#include "Waveform.h"

namespace wave
{
	struct waveform_impl : waveform
	{
		virtual bool get_field(pfc::string const& what, unsigned index, pfc::list_base_t<float>& out) override;
		virtual unsigned get_channel_count() const override;
		virtual unsigned get_channel_map() const override;

		pfc::list_t<pfc::list_t<float>> minimum, maximum, rms;
		unsigned channel_map;
	};
}