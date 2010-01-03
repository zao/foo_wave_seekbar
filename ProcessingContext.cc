#include "PchCache.h"
#include "ProcessingContext.h"
#include "Cache.h"

namespace wave
{
	unsigned processing_contextmenu_item::get_num_items()
	{
		return 1;
	}

	void processing_contextmenu_item::get_item_name(unsigned p_index, pfc::string_base& p_out)
	{
		p_out = "Extract Seekbar Signature";
	}

	void processing_contextmenu_item::get_item_default_path(unsigned p_index, pfc::string_base& p_out)
	{

	}

	void enqueue(static_api_ptr_t<cache>& cache, const metadb_handle_ptr& p)
	{
		cache->enqueue_waveform(p->get_location(), true);
	}

	void processing_contextmenu_item::context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller)
	{
		static_api_ptr_t<cache> infoCache;
		p_data.enumerate(boost::bind(&enqueue, infoCache, _1));
	}

	GUID processing_contextmenu_item::get_item_guid(unsigned p_index)
	{
		return extract_guid;
	}

	bool processing_contextmenu_item::get_item_description(unsigned p_index, pfc::string_base& p_out)
	{
		p_out = "Extracts a signature suitable for consumption by the waveform seekbar.";
		return true;
	}

	// {3950A2FA-7FCB-4680-829F-7FC51EC159A0}
	const GUID processing_contextmenu_item::extract_guid = 
	{ 0x3950a2fa, 0x7fcb, 0x4680, { 0x82, 0x9f, 0x7f, 0xc5, 0x1e, 0xc1, 0x59, 0xa0 } };
}

static contextmenu_item_factory_t<wave::processing_contextmenu_item> g_asdf;