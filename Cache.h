#pragma once

#include <vector>

namespace wave {
	struct waveform : service_base
	{
		virtual bool get_field(pfc::string const& what, pfc::list_base_t<float>& out) abstract;
		FB2K_MAKE_SERVICE_INTERFACE(waveform, service_base)
	};

	service_ptr_t<waveform> make_placeholder_waveform();

	struct get_response
	{
		service_ptr_t<waveform> waveform;
	};

	struct get_request
	{
		get_request()
			: completion_handler([](shared_ptr<get_response>) {})
		{}
		playable_location_impl location;
		bool user_requested;
		function<void (shared_ptr<get_response>)> completion_handler;
	};

	struct cache : service_base
	{
		virtual void get_waveform(shared_ptr<get_request> request) abstract;
		virtual void remove_dead_waveforms() abstract;
		virtual void compact_storage() abstract;

		virtual void flush() abstract;

		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(cache)
	};


	// {5A1DFE0F-B6B8-4891-9798-16230D8C4D21}
	__declspec(selectany) const GUID cache::class_guid = 
	{ 0x5a1dfe0f, 0xb6b8, 0x4891, { 0x97, 0x98, 0x16, 0x23, 0xd, 0x8c, 0x4d, 0x21 } };

	// {53CA29B6-CF63-44ce-BF20-1532BEADFD62}
	__declspec(selectany) const GUID waveform::class_guid = 
	{ 0x53ca29b6, 0xcf63, 0x44ce, { 0xbf, 0x20, 0x15, 0x32, 0xbe, 0xad, 0xfd, 0x62 } };

}