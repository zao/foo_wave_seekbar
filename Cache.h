//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <vector>
#include "waveform_sdk/Waveform.h"

namespace wave
{
	struct waveform_query : service_base
	{
		enum query_urgency
		{
			needed_urgency,
			desired_urgency,
			bulk_urgency
		};

		enum query_force
		{
			unforced_query,
			forced_query
		};

		virtual playable_location const& get_location() const = 0;
		virtual query_urgency get_urgency() const = 0;
		virtual query_force get_forced() const = 0;
		virtual float get_progress() const = 0;
		virtual ref_ptr<waveform> get_waveform() const = 0;
		virtual void abort() = 0;

		virtual void set_waveform(ref_ptr<waveform>, float progress) = 0;

		FB2K_MAKE_SERVICE_INTERFACE(waveform_query, service_base);
	};

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
		virtual service_ptr_t<waveform_query> create_query(playable_location const& loc,
			waveform_query::query_urgency urgency, waveform_query::query_force forced) = 0;
		virtual service_ptr_t<waveform_query> create_callback_query(playable_location const& loc,
			waveform_query::query_urgency urgency, waveform_query::query_force forced,
			std::function<void(service_ptr_t<waveform_query>)> callback) = 0;
		virtual void get_waveform(service_ptr_t<waveform_query> request) abstract;
		virtual void remove_dead_waveforms() abstract;
		virtual void compact_storage() abstract;
		virtual void rescan_waveforms() abstract;

		virtual bool has_waveform(playable_location const& loc) abstract;
		virtual void remove_waveform(playable_location const& loc) abstract;

		virtual void defer_action(std::function<void ()> fun) abstract;

		virtual bool is_location_forbidden(playable_location const& loc) abstract;
		virtual bool get_waveform_sync(playable_location const& loc, ref_ptr<waveform>& out) abstract;
		
		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(cache)
	};

	// {E2AB1144-58A1-4DED-A2FB-7BBE74B38D5F}
	__declspec(selectany) const GUID waveform_query::class_guid =
	{ 0xe2ab1144, 0x58a1, 0x4ded, { 0xa2, 0xfb, 0x7b, 0xbe, 0x74, 0xb3, 0x8d, 0x5f } };


	// {46844662-C924-402B-A0F9-A062B2074698}
	__declspec(selectany) const GUID cache::class_guid =
	{ 0x46844662, 0xc924, 0x402b, { 0xa0, 0xf9, 0xa0, 0x62, 0xb2, 0x7, 0x46, 0x98 } };
}
