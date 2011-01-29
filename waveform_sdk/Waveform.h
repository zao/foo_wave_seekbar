#pragma once
#include "../../SDK/foobar2000.h"
#include <vector>

namespace wave
{
	struct waveform : service_base
	{
		virtual bool get_field(pfc::string const& what, unsigned index, pfc::list_base_t<float>& out) abstract;
		virtual unsigned get_channel_count() const abstract;
		virtual unsigned get_channel_map() const abstract;

		FB2K_MAKE_SERVICE_INTERFACE(waveform, service_base)
	};
	
	service_ptr_t<waveform> make_placeholder_waveform();
	service_ptr_t<waveform> downmix_waveform(service_ptr_t<waveform> const& in);

	// {53CA29B6-CF63-44ce-BF20-1532BEADFD62}
	__declspec(selectany) const GUID waveform::class_guid = 
	{ 0x53ca29b6, 0xcf63, 0x44ce, { 0xbf, 0x20, 0x15, 0x32, 0xbe, 0xad, 0xfd, 0x62 } };
}