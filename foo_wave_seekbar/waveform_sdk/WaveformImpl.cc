//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "WaveformImpl.h"
#include <cassert>

namespace wave
{
	bool waveform_impl::get_field(char const* what, unsigned index, array_sink<float> const& out)
	{
		auto I = fields.find(what);
		if (!I.is_valid())
			return false;

		auto& field = I->m_value;
		if (index >= field.get_size())
			return false;

		pfc::list_t<float> data;
		data.add_items(field[index]);
		out.set(data.get_ptr(), data.get_count());
		return true;
	}

	unsigned waveform_impl::get_channel_count() const
	{
		if (fields.get_count() == 0)
			throw std::runtime_error("channel count query on empty waveform");
		return (unsigned)fields.first()->m_value.get_count();
	}

	unsigned waveform_impl::get_channel_map() const
	{
		return channel_map;
	}

	ref_ptr<waveform> waveform_impl::clone() const
	{
		waveform_impl* out = new waveform_impl;
		out->channel_map = channel_map;
		out->fields = fields;
		return ref_ptr<waveform>(out);
	}
}
