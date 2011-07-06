//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "Waveform.h"

namespace wave
{
	struct waveform_impl : waveform
	{
		virtual bool get_field(pfc::string const& what, unsigned index, pfc::list_base_t<float>& out) override;
		virtual unsigned get_channel_count() const override;
		virtual unsigned get_channel_map() const override;

		pfc::map_t<pfc::string, pfc::list_t<pfc::list_hybrid_t<float, 2048>>> fields;
		unsigned channel_map;
	};
}
