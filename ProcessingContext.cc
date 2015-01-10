//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "ProcessingContext.h"
#include "Cache.h"
#include <list>
#include <map>

namespace wave
{
	unsigned processing_contextmenu_item::get_num_items()
	{
		return 4;
	}

	void processing_contextmenu_item::get_item_name(unsigned p_index, pfc::string_base& p_out)
	{
		if (p_index == 0)
			p_out = "Extract waveform signature if missing";
		if (p_index == 1)
			p_out = "Force-extract waveform signature";
		if (p_index == 2)
			p_out = "Remove waveform signature";
		if (p_index == 3)
			p_out = "Submit cache priority test";
	}

	void processing_contextmenu_item::get_item_default_path(unsigned p_index, pfc::string_base& p_out)
	{
	}

	void enqueue(service_ptr_t<cache> cache, std::shared_ptr<std::vector<playable_location_impl>> locs,
		waveform_query::query_force forced)
	{
		for (auto I = locs->begin(); I != locs->end(); ++I)
		{
			auto q = cache->create_query(*I, waveform_query::bulk_urgency, forced);
			cache->get_waveform(q);
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
			infoCache->defer_action(std::bind(&enqueue, infoCache, locs, waveform_query::unforced_query));
		if (p_index == 1)
			infoCache->defer_action(std::bind(&enqueue, infoCache, locs, waveform_query::forced_query));
		if (p_index == 2)
			infoCache->defer_action(std::bind(&remove, infoCache, locs));
		if (p_index == 3) {
			std::map<waveform_query::query_urgency, std::list<service_ptr_t<waveform_query> > > bins;
			for (size_t i = 0; i < locs->size(); ++i) {
				auto loc = (*locs)[i];
				auto urgency = waveform_query::bulk_urgency;
				if (i == 3) {
					urgency = waveform_query::desired_urgency;
				}
				if (i == 5) {
					urgency = waveform_query::needed_urgency;
				}
				auto q = infoCache->create_query(loc, urgency, waveform_query::forced_query);
				bins[urgency].push_back(q);
			}
			for (auto I = bins.rbegin(); I != bins.rend(); ++I) {
				for (auto J = I->second.begin(); J != I->second.end(); ++J) {
					infoCache->get_waveform(*J);
				}
			}
		}
	}

	GUID processing_contextmenu_item::get_item_guid(unsigned p_index)
	{
		if (p_index == 0)
			return extract_guid;
		if (p_index == 1)
			return force_extract_guid;
		if (p_index == 2)
			return remove_guid;
		return test_guid;
	}

	bool processing_contextmenu_item::get_item_description(unsigned p_index, pfc::string_base& p_out)
	{
		if (p_index == 0)
			p_out = "Extracts a signature suitable for consumption by the waveform seekbar if not already present.";
		if (p_index == 1)
			p_out = "Extracts a signature suitable for consumption by the waveform seekbar, overwriting any previous signature.";
		if (p_index == 2)
			p_out = "Removes the signature from the waveform database, if present.";
		if (p_index == 3)
			p_out = "Test stuff.";
		return true;
	}

	// {3950A2FA-7FCB-4680-829F-7FC51EC159A0}
	const GUID processing_contextmenu_item::extract_guid =
	{ 0x3950a2fa, 0x7fcb, 0x4680, { 0x82, 0x9f, 0x7f, 0xc5, 0x1e, 0xc1, 0x59, 0xa0 } };

	// {CAD45EBC-B6FD-4AC5-A734-23E491054AC6}
	const GUID processing_contextmenu_item::force_extract_guid =
	{ 0xcad45ebc, 0xb6fd, 0x4ac5, { 0xa7, 0x34, 0x23, 0xe4, 0x91, 0x5, 0x4a, 0xc6 } };

	// {AF04D9DF-6C2B-4E70-AC05-0E3691B83224}
	const GUID processing_contextmenu_item::remove_guid =
	{ 0xaf04d9df, 0x6c2b, 0x4e70, { 0xac, 0x5, 0xe, 0x36, 0x91, 0xb8, 0x32, 0x24 } };

	// {483EFC09-9738-48FA-AE1A-81A86F168614}
	const GUID processing_contextmenu_item::test_guid =
	{ 0x483efc09, 0x9738, 0x48fa, { 0xae, 0x1a, 0x81, 0xa8, 0x6f, 0x16, 0x86, 0x14 } };


	GUID processing_contextmenu_item::get_parent()
	{
		return contextmenu_groups::utilities;
	}
}

static contextmenu_item_factory_t<wave::processing_contextmenu_item> g_asdf;
