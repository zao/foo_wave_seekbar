#include "PchSeekbar.h"
#include "Cache.h"

// {64482E5D-6DF6-4A80-BD0A-25B06F2BE585}
static GUID const guid_cache_group =
{ 0x64482e5d, 0x6df6, 0x4a80, { 0xbd, 0xa, 0x25, 0xb0, 0x6f, 0x2b, 0xe5, 0x85 } };


struct cache_commands : mainmenu_commands
{
	virtual t_uint32 get_command_count() { return 3; }
	virtual GUID get_command(t_uint32 index)
	{
		// {C001F96F-62D2-4248-A50A-E26846D7CCEC}
		static const GUID purge_guid = 
		{ 0xc001f96f, 0x62d2, 0x4248, { 0xa5, 0xa, 0xe2, 0x68, 0x46, 0xd7, 0xcc, 0xec } };
		
		// {BC55F84B-958B-4afb-9FBA-EC91EDDD734C}
		static const GUID compact_guid = 
		{ 0xbc55f84b, 0x958b, 0x4afb, { 0x9f, 0xba, 0xec, 0x91, 0xed, 0xdd, 0x73, 0x4c } };
		
		// {89B0F429-C749-4027-BEED-D9BE07FC67C5}
		static const GUID rescan_guid = 
		{ 0x89b0f429, 0xc749, 0x4027, { 0xbe, 0xed, 0xd9, 0xbe, 0x7, 0xfc, 0x67, 0xc5 } };

		GUID const* guids[] = { &purge_guid, &compact_guid, &rescan_guid };
		return *guids[index];
	}
	virtual void get_name(t_uint32 index, pfc::string_base& out)
	{
		switch (index)
		{
			case 0: out = "Remove Dead Waveforms"; break;
			case 1: out = "Compact Waveform Database"; break;
			case 2: out = "Rescan All Waveforms"; break;
		}
	}
	virtual bool get_description(t_uint32 index, pfc::string_base& out)
	{
		switch (index)
		{
			case 0: out = "Removes dead waveforms from the Waveform Cache database."; break;
			case 1: out = "Compacts the waveform database, may take a while."; break;
			case 2: out = "Enqueue all waveforms in the database for signature extraction."; break;
		}
		return true;
	}
	virtual GUID get_parent() { return guid_cache_group; }
	virtual void execute(t_uint32 index, service_ptr_t<service_base> callback)
	{
		static_api_ptr_t<wave::cache> c;
		switch (index)
		{
			case 0:
			{
				c->remove_dead_waveforms();
				break;
			}
			case 1:
			{
				c->compact_storage();
				break;
			}
			case 2:
			{
				c->rescan_waveforms();
				break;
			}
		}
	}

};

static mainmenu_group_popup_factory g_sadf(guid_cache_group, mainmenu_groups::library, mainmenu_commands::sort_priority_base, "Waveform Seekbar");
static mainmenu_commands_factory_t<cache_commands> g_asdf;