//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "Waveform.h"

namespace wave {
struct waveform_impl : waveform
{
    virtual bool get_field(char const* what, unsigned index, array_sink<float> const& out) override;
    virtual unsigned get_channel_count() const override;
    virtual unsigned get_channel_map() const override;
    virtual ref_ptr<waveform> clone() const override;

    typedef pfc::list_t<float> signal;
    typedef pfc::list_t<signal> bundle;

    pfc::map_t<pfc::string, bundle> fields;
    unsigned channel_map;
};
}
