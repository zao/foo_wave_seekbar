#pragma once
#pragma warning(disable: 4005)
#define D3D_DEBUG_INFO
#define _WIN32_WINNT 0x0501

#define NOMINMAX
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/assign.hpp>
#include <boost/cstdint.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
using boost::scoped_ptr;
using boost::shared_ptr;
typedef boost::recursive_mutex::scoped_lock scoped_lock;

using boost::int8_t;
using boost::int16_t;
using boost::int32_t;
using boost::int64_t;
using boost::uint8_t;
using boost::uint16_t;
using boost::uint32_t;
using boost::uint64_t;

using boost::function;

#pragma warning(push)
#pragma warning(disable: 4244)
#include <boost/iostreams/filtering_stream.hpp>
#pragma warning(pop)
#include <boost/range/iterator_range.hpp>
namespace io = boost::iostreams;

#include <algorithm>
using std::min;
using std::max;

#include <comdef.h>
#include <d3d10_1.h>
#include <d3dx10.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
#include <tchar.h>

#include "../ATLHelpers/ATLHelpers.h"
#include <atlframe.h>
#include <atlcrack.h>
#include "../columns_ui-sdk/ui_extension.h"
#include <dwmapi.h>

#include <map>
#include <vector>

using namespace boost::assign;

using boost::noncopyable;

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
using boost::scoped_ptr;
using boost::shared_ptr;
using boost::weak_ptr;

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/version.hpp>

#include <boost/cstdint.hpp>

using boost::uint8_t;
using boost::uint16_t;
using boost::uint32_t;

#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>

using boost::enable_shared_from_this;
using boost::optional;


#include <delayimp.h>

#include "sqlite3.h"

#undef SelectBitmap
#undef SelectBrush
#undef SelectPen