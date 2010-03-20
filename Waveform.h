#pragma once

namespace wave
{
	struct waveform : service_base
	{
		virtual bool get_field(pfc::string const& what, unsigned channel, pfc::list_base_t<float>& out) abstract;
		virtual unsigned get_channel_count() const abstract;
		virtual unsigned get_channel_map() const abstract;
		FB2K_MAKE_SERVICE_INTERFACE(waveform, service_base)
	};
	
	service_ptr_t<waveform> make_placeholder_waveform();
}