#pragma once

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501
#endif

#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#if !defined(ASIO_NO_WIN32_LEAN_AND_MEAN)
#define ASIO_NO_WIN32_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <asio.hpp>