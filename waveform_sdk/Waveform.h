//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../../SDK/foobar2000.h"
#include <vector>

namespace wave
{
	namespace waveform
	{
#pragma warning(push)
#pragma warning(disable: 4200)
		struct data
		{
			unsigned channel_count, channel_map;
			float storage[0];
		};
#pragma warning(pop)

		enum field_tag { min_field, max_field, rms_field };

		data* create(unsigned channel_map);
		void destroy(data*);

		float* get_field(data*, unsigned channel_idx, field_tag);
		float const* get_field(data const*, unsigned channel_idx, field_tag);
		unsigned get_channel_count(data const*);
		unsigned get_channel_map(data const*);

		data* make_placeholder();
		data* downmix(data const* in);
	}
}
