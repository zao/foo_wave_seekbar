//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#pragma warning(disable: 4005)
#define D3D_DEBUG_INFO

#include <algorithm>
using std::min;
using std::max;
#include <stdint.h>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <tuple>
#include <vector>

#include <comdef.h>
#include <tchar.h>

#include <ATLHelpers/ATLHelpers.h>
#include <atlframe.h>
#include <atlcrack.h>
#include <columns_ui-sdk/ui_extension.h>
#include <dwmapi.h>

#include <delayimp.h>

#include "sqlite3.h"

#undef SelectBitmap
#undef SelectBrush
#undef SelectPen
