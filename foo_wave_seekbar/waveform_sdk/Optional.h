//          Copyright Lars Viklund 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace wave {
template<typename T>
struct optional
{
    optional()
      : laden(false)
    {}

    optional(T const& t)
      : t(t)
      , laden(true)
    {}

    bool valid() const { return laden; }
    T const& operator*() const { return t; }
    T& operator*() { return t; }

    optional& operator=(T const& t)
    {
        this->t = t;
        laden = true;
        return *this;
    }
    void reset() { laden = false; }

    bool operator==(optional const& rhs) const
    {
        if (laden && rhs.laden) {
            return !!(t == rhs.t);
        }
        return (!laden && !rhs.laden);
    }

    bool operator!=(optional const& rhs) const { return !(*this == rhs); }

  private:
    T t;
    bool laden;
};
}