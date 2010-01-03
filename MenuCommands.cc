#include "PchCache.h"

struct cache_commands : mainmenu_commands
{
	virtual t_uint32 get_command_count() { return 2; }
	virtual GUID get_command(t_uint32 index)
	{
		// {C001F96F-62D2-4248-A50A-E26846D7CCEC}
		static const GUID purge_guid = 
		{ 0xc001f96f, 0x62d2, 0x4248, { 0xa5, 0xa, 0xe2, 0x68, 0x46, 0xd7, 0xcc, 0xec } };
		// {BC55F84B-958B-4afb-9FBA-EC91EDDD734C}
		static const GUID compact_guid = 
		{ 0xbc55f84b, 0x958b, 0x4afb, { 0x9f, 0xba, 0xec, 0x91, 0xed, 0xdd, 0x73, 0x4c } };

		GUID const* guids[] = { &purge_guid, &compact_guid };
		return *guids[index];
	}
	virtual void get_name(t_uint32 index, pfc::string_base& out)
	{
		switch (index)
		{
			case 0: out = "Remove Dead Waveforms"; break;
			case 1: out = "Compact Waveform Database"; break;
		}
	}
	virtual bool get_description(t_uint32 index, pfc::string_base& out)
	{
		switch (index)
		{
			case 0: out = "Removes dead waveforms from the Waveform Cache database."; break;
			case 1: out = "Compacts the waveform database, may take a while."; break;
		}
		return true;
	}
	virtual GUID get_parent() { return mainmenu_groups::library; }
	virtual void execute(t_uint32 index, service_ptr_t<service_base> callback)
	{
		static_api_ptr_t<cache> c;
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
		}
	}

};

static mainmenu_commands_factory_t<cache_commands> g_asdf;