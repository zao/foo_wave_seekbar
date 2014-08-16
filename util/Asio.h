//          Copyright Lars Viklund 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501
#endif

#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif

#if !defined(BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN)
#define BOOST_ASIO_NO_WIN32_LEAN_AND_MEAN 1
#endif

#if !defined(BOOST_ASIO_DISABLE_IOCP)
#define BOOST_ASIO_DISABLE_IOCP 1
#endif

#include <algorithm>
#include <list>
#include <string>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

struct asio_worker_pool : boost::noncopyable
{
	asio_worker_pool(size_t num_workers, std::string thread_basename);
	~asio_worker_pool();

	template <typename NullaryCallable>
	void post(NullaryCallable callable)
	{
		io.post(callable);
	}

private:
	boost::asio::io_service io;
	boost::asio::io_service::work* work;
	std::list<boost::shared_ptr<boost::thread>> threads;
};
