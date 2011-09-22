//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#define NOMINMAX
using std::min; using std::max;

#include <boost/assign.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>

using boost::noncopyable;
using boost::shared_ptr;
using boost::weak_ptr;

#include <atlbase.h>
#include <atlapp.h>
#include <atlcom.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlstr.h>

#include <d3d9.h>
#include <d3dx9.h>

