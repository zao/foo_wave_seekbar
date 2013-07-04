//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <vector>
#include "waveform_sdk/Waveform.h"

namespace wave
{
	struct get_response
	{
		get_response() : valid_bucket_count(2048) {}
		ref_ptr<waveform> waveform;
		size_t valid_bucket_count;
	};

	struct get_request
	{
		get_request()
			: completion_handler([](std::shared_ptr<get_response>) {})
		{}
		playable_location_impl location;
		bool user_requested;
		std::function<void (std::shared_ptr<get_response>)> completion_handler;
	};

	struct cache : service_base
	{
		virtual void get_waveform(std::shared_ptr<get_request> request) abstract;
		virtual void remove_dead_waveforms() abstract;
		virtual void compact_storage() abstract;
		virtual void rescan_waveforms() abstract;

		virtual void flush() abstract;

		virtual bool has_waveform(playable_location const& loc) abstract;
		virtual void remove_waveform(playable_location const& loc) abstract;

		virtual void defer_action(std::function<void ()> fun) abstract;

		virtual bool is_location_forbidden(playable_location const& loc) abstract;
		virtual bool get_waveform_sync(playable_location const& loc, ref_ptr<waveform>& out) abstract;
		
		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(cache)
	};

	// {46844662-C924-402B-A0F9-A062B2074698}
	__declspec(selectany) const GUID cache::class_guid = 
	{ 0x46844662, 0xc924, 0x402b, { 0xa0, 0xf9, 0xa0, 0x62, 0xb2, 0x7, 0x46, 0x98 } };

}
