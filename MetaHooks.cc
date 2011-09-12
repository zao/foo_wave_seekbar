//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "../SDK/foobar2000.h"
#include "Cache.h"

struct waveform_hooks : metadb_display_field_provider
{
	virtual t_uint32 get_field_count() { return 1; }
	virtual void get_field_name(t_uint32 idx, pfc::string_base& out)
	{
		if (idx == 0)
			out = "waveform_channel_count";
	}

	virtual bool process_field(t_uint32 idx, metadb_handle* meta, titleformat_text_out* dst)
	{
		if (idx == 0)
		{
			auto const& loc = meta->get_location();
			static_api_ptr_t<wave::cache_v4> c;
			wave::waveform_info info = {};
			if (!c->get_waveform_info(loc, info))
				return false;

			dst->write_int(titleformat_inputtypes::unknown, info.channel_count);
			return true;
		}
		return false;
	}
};

static service_factory_t<waveform_hooks> g_asdf;
