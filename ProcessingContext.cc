//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "ProcessingContext.h"
#include "Cache.h"

namespace wave
{
	unsigned processing_contextmenu_item::get_num_items()
	{
		return 2;
	}

	void processing_contextmenu_item::get_item_name(unsigned p_index, pfc::string_base& p_out)
	{
		if (p_index == 0)
			p_out = "Extract Seekbar Signature";
		if (p_index == 1)
			p_out = "Remove Seekbar Signature";
	}

	void processing_contextmenu_item::get_item_default_path(unsigned p_index, pfc::string_base& p_out)
	{
	}

	void enqueue(service_ptr_t<cache> cache, std::shared_ptr<std::vector<playable_location_impl>> locs)
	{
		for (auto I = locs->begin(); I != locs->end(); ++I)
		{
			auto request = std::make_shared<get_request>();
			auto const& loc = *I;
			request->location = loc;
			request->user_requested = true;
			cache->get_waveform(request);
		}
	}

	void remove(service_ptr_t<cache> cache, std::shared_ptr<std::vector<playable_location_impl>> locs)
	{
		for (auto I = locs->begin(); I != locs->end(); ++I)
		{
			auto const& loc = *I;
			cache->remove_waveform(loc);
		}
	}

	void processing_contextmenu_item::context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller)
	{
		auto infoCache = standard_api_create_t<cache>();
		auto locs = std::make_shared<std::vector<playable_location_impl>>();
		locs->reserve(p_data.get_size());
		p_data.enumerate([&](metadb_handle_ptr p)
		{
			locs->push_back(p->get_location());
		});
		if (p_index == 0)
			infoCache->defer_action(std::bind(&enqueue, infoCache, locs));
		if (p_index == 1)
			infoCache->defer_action(std::bind(&remove, infoCache, locs));
	}

	GUID processing_contextmenu_item::get_item_guid(unsigned p_index)
	{
		if (p_index == 0)
			return extract_guid;
		return remove_guid;
	}

	bool processing_contextmenu_item::get_item_description(unsigned p_index, pfc::string_base& p_out)
	{
		if (p_index == 0)
			p_out = "Extracts a signature suitable for consumption by the waveform seekbar.";
		if (p_index == 1)
			p_out = "Removes the signature from the waveform database, if any.";
		return true;
	}

	// {3950A2FA-7FCB-4680-829F-7FC51EC159A0}
	const GUID processing_contextmenu_item::extract_guid = 
	{ 0x3950a2fa, 0x7fcb, 0x4680, { 0x82, 0x9f, 0x7f, 0xc5, 0x1e, 0xc1, 0x59, 0xa0 } };

	// {AF04D9DF-6C2B-4E70-AC05-0E3691B83224}
	const GUID processing_contextmenu_item::remove_guid = 
	{ 0xaf04d9df, 0x6c2b, 0x4e70, { 0xac, 0x5, 0xe, 0x36, 0x91, 0xb8, 0x32, 0x24 } };

	GUID processing_contextmenu_item::get_parent()
	{
		return contextmenu_groups::utilities;
	}
}

static contextmenu_item_factory_t<wave::processing_contextmenu_item> g_asdf;
