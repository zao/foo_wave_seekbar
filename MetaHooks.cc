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
			static_api_ptr_t<wave::cache_v2> c;
			if (!c->has_waveform(loc))
				return false;

			shared_ptr<wave::get_request> req = make_shared<wave::get_request>();
			req->location = loc;
			req->user_requested = false;
			shared_ptr<int> channel_count = make_shared<int>();
			req->completion_handler = [channel_count](shared_ptr<wave::get_response> resp)
			{
				*channel_count = resp
					? get_channel_count(resp.get())
					: 0;
			};

			c->get_waveform(req);
			dst->write_int(titleformat_inputtypes::unknown, *channel_count);
			return !!*channel_count;
		}
		return false;
	}
};

static service_factory_t<waveform_hooks> g_asdf;
