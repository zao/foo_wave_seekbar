//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "WaveformImpl.h"
#include <cassert>

namespace wave
{
	bool waveform_impl::get_field(pfc::string const& what, unsigned index, pfc::list_base_t<float>& out)
	{
		auto I = fields.find(what);
		if (!I.is_valid())
			return false;

		auto& field = I->m_value;
		if (index >= field.get_size())
			return false;
		
		out = field[index];
		return true;
	}

	unsigned waveform_impl::get_channel_count() const
	{
		if (fields.get_count() == 0)
			throw std::runtime_error("channel count query on empty waveform");
		return fields.first()->m_value.get_count();
	}

	unsigned waveform_impl::get_channel_map() const
	{
		return channel_map;
	}
}
