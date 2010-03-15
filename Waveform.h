#pragma once

namespace wave
{
	struct waveform : service_base
	{
		virtual bool get_field(pfc::string const& what, pfc::list_base_t<float>& out) abstract;
		FB2K_MAKE_SERVICE_INTERFACE(waveform, service_base)
	};
	
	service_ptr_t<waveform> make_placeholder_waveform();
}