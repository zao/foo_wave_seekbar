//          Copyright Lars Viklund 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "util/Asio.h"
#include "Helpers.h"
#include <Objbase.h>

#include <boost/make_shared.hpp>
#include <boost/thread.hpp>

struct idle_priority_scope : boost::noncopyable
{
	idle_priority_scope()
	{
		HANDLE this_thread = GetCurrentThread();
		old_priority = GetThreadPriority(this_thread);
		SetThreadPriority(this_thread, THREAD_PRIORITY_IDLE);
		CloseHandle(this_thread);
	}

	~idle_priority_scope()
	{
		HANDLE this_thread = GetCurrentThread();
		SetThreadPriority(this_thread, old_priority);
		CloseHandle(this_thread);
	}

private:
	int old_priority;
};

struct with_idle_priority
{
	typedef boost::function<void()> function_type;
	with_idle_priority(function_type func)
		: func(func)
	{}

	void operator ()()
	{
		idle_priority_scope ips;
		func();
	}

	function_type func;
};

struct coinitialize_scope : boost::noncopyable
{
	coinitialize_scope()
	{
		CoInitialize(nullptr);
	}

	~coinitialize_scope()
	{
		CoUninitialize();
	}
};

asio_worker_pool::asio_worker_pool(size_t num_cores, std::string thread_basename)
{
	work = new boost::asio::io_service::work(io);
	for (size_t i = 0; i < num_cores; ++i) {
		auto fun = [=]
		{
			idle_priority_scope ips;
			coinitialize_scope cis;
			char name_buf[128] = {};
			sprintf_s(name_buf, "%s-%d/%d", thread_basename.c_str(), i + 1, num_cores);
			::SetThreadName(-1, name_buf);
			io.run();
		};
		auto thread_ptr = boost::make_shared<boost::thread>(fun);
		threads.push_back(thread_ptr);
	}
}

asio_worker_pool::~asio_worker_pool()
{
	delete work;
	for (auto I = threads.begin(); I != threads.end(); ++I)
	{
		(*I)->join();
	}
}