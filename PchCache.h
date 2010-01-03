#pragma once
#define _WIN32_WINNT 0x0501

#define NOMINMAX
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp>
#include <boost/assign.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
using boost::scoped_ptr;
using boost::shared_ptr;

#include <algorithm>
using std::min;
using std::max;

#include "../SDK/foobar2000.h"

#include <map>
#include <vector>

using namespace boost::assign;

#include <boost/algorithm/string.hpp>

using boost::noncopyable;

#include "sqlite3.h"