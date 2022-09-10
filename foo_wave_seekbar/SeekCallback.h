//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace wave {
struct seek_callback
{
    virtual ~seek_callback() {}
    virtual void on_seek_begin() abstract;
    virtual void on_seek_position(double time, bool legal) abstract;
    virtual void on_seek_end(bool aborted) abstract;
};
}
