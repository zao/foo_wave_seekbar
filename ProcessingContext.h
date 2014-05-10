//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../SDK/foobar2000.h"

namespace wave
{
	struct processing_contextmenu_item : contextmenu_item_simple
	{
		virtual unsigned get_num_items();
		virtual void get_item_name(unsigned p_index, pfc::string_base& p_out);
		virtual void get_item_default_path(unsigned p_index, pfc::string_base& p_out);
		virtual void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller);
		virtual GUID get_item_guid(unsigned p_index);
		virtual bool get_item_description(unsigned p_index, pfc::string_base& p_out);

		virtual GUID get_parent();

		static const GUID extract_guid;
		static const GUID remove_guid;
	};
}
