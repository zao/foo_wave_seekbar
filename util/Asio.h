#pragma once

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501
#endif

#if !defined(BOOST_SPIRIT_USE_PHOENIX_V3)
#define BOOST_SPIRIT_USE_PHOENIX_V3
#endif

#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#if !defined(BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN)
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN
#endif

#include <boost/asio.hpp>