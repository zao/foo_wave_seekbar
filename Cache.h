//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <vector>
#include "waveform_sdk/Waveform.h"
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace wave {
	using boost::function;
	using boost::shared_ptr;

	struct get_response
	{
		get_response() : valid_bucket_count(2048) {}
		ref_ptr<waveform> waveform;
		size_t valid_bucket_count;
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
		virtual void rescan_waveforms() abstract;

		virtual void flush() abstract;

		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(cache)
	};

	struct cache_v2 : cache
	{
		virtual bool has_waveform(playable_location const& loc) abstract;
		virtual void remove_waveform(playable_location const& loc) abstract;

		FB2K_MAKE_SERVICE_INTERFACE(cache_v2, cache)
	};

	struct cache_v3 : cache_v2
	{
		virtual void compression_bench() abstract;

		FB2K_MAKE_SERVICE_INTERFACE(cache_v3, cache_v2)
	};

	struct cache_v4 : cache_v3
	{
		virtual void defer_action(boost::function<void ()> fun) abstract;

		FB2K_MAKE_SERVICE_INTERFACE(cache_v4, cache_v3)
	};

	struct cache_v5 : cache_v4
	{
		virtual bool is_location_forbidden(playable_location const& loc) abstract;
		virtual bool get_waveform_sync(playable_location const& loc, ref_ptr<waveform>& out) abstract;

		FB2K_MAKE_SERVICE_INTERFACE(cache_v5, cache_v4)
	};

	// {A4F5529F-752F-4F13-B703-FD938C211E7D}
	__declspec(selectany) const GUID cache_v5::class_guid = 
	{ 0xa4f5529f, 0x752f, 0x4f13, { 0xb7, 0x3, 0xfd, 0x93, 0x8c, 0x21, 0x1e, 0x7d } };

	// {0E58FDB7-0B5B-4727-968A-ED2EFCAE75AA}
	__declspec(selectany) const GUID cache_v4::class_guid = 
	{ 0xe58fdb7, 0xb5b, 0x4727, { 0x96, 0x8a, 0xed, 0x2e, 0xfc, 0xae, 0x75, 0xaa } };
	
	// {503C40C1-ED5B-492E-98D7-DE9EB63D2F81}
	__declspec(selectany) const GUID cache_v3::class_guid = 
	{ 0x503c40c1, 0xed5b, 0x492e, { 0x98, 0xd7, 0xde, 0x9e, 0xb6, 0x3d, 0x2f, 0x81 } };

	// {195F8048-1CF9-467A-AC72-5811E0636D77}
	__declspec(selectany) const GUID cache_v2::class_guid = 
	{ 0x195f8048, 0x1cf9, 0x467a, { 0xac, 0x72, 0x58, 0x11, 0xe0, 0x63, 0x6d, 0x77 } };
	
	// {5A1DFE0F-B6B8-4891-9798-16230D8C4D21}
	__declspec(selectany) const GUID cache::class_guid = 
	{ 0x5a1dfe0f, 0xb6b8, 0x4891, { 0x97, 0x98, 0x16, 0x23, 0xd, 0x8c, 0x4d, 0x21 } };
}
