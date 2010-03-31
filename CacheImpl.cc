#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include <boost/format.hpp>

namespace wave
{
	cache_impl::cache_impl()
		: idle_work(new boost::asio::io_service::work(io)), initialized(0)
	{
	}

	cache_impl::~cache_impl()
	{
	}

	struct with_idle_priority
	{
		typedef boost::function<void ()> function_type;
		with_idle_priority(function_type func)
		: func(func)
		{}

		void operator ()()
		{
			HANDLE this_thread = GetCurrentThread();
			SetThreadPriority(this_thread, THREAD_PRIORITY_IDLE);
			CloseHandle(this_thread);
			func();
		}

		function_type func;
	};

	void cache_impl::load_data(shared_ptr<boost::barrier> load_barrier)
	{
		open_store();

		std::deque<job> jobs;
		if (store)
		{
			store->get_jobs(jobs);
			for each (job j in jobs)
			{
				shared_ptr<get_request> request(new get_request);
				request->location.copy(j.loc);
				request->user_requested = j.user;
				io.post([this, request]()
				{
					get_waveform(request);
				});
			}
		}
		else
		{
			console::warning("Wave cache: could not open backing database.");
		}
		load_barrier->wait();
	}

	void cache_impl::flush()
	{
		{
			boost::mutex::scoped_lock(cache_mutex);
			flush_callback.abort();
		}
		idle_work.reset();
		work_threads.join_all();

		open_store();
		if (store)
		{
			store->put_jobs(job_flush_queue);
		}

		store.reset();
	}

	void cache_impl::open_store()
	{
		if (store) return;
		cache_filename = core_api::get_profile_path();
		cache_filename += "\\wavecache.db";
		cache_filename = cache_filename.subString(7);		
		store.reset(new backing_store(cache_filename));
	}

	void cache_impl::delayed_init()
	{
		shared_ptr<boost::barrier> load_barrier(new boost::barrier(2));
		io.post([this, load_barrier]()
		{
			load_data(load_barrier);
		});
		size_t n_cores = boost::thread::hardware_concurrency();
		for (size_t i = 0; i < std::min(3u, n_cores); ++i) {
			work_threads.create_thread(with_idle_priority([this]()
			{
				this->io.run();
			}));
			if (!i)
			{
				load_barrier->wait();
			}
		}
	}

	void cache_impl::get_waveform(shared_ptr<get_request> request)
	{
		boost::mutex::scoped_lock sl(cache_mutex);
		if (!InterlockedCompareExchange(&initialized, 1, 0))
		{
			delayed_init();
		}

		shared_ptr<get_response> response(new get_response);
		if (!request->user_requested && store && store->get(response->waveform, request->location))
		{
			request->completion_handler(response);
		}
		else
		{
			response->waveform = make_placeholder_waveform();
			request->completion_handler(response);
			
			response.reset(new get_response);
			if (!request->user_requested)
			{
				important_queue.push(request->location);
			}
			io.post([this, request, response]()
			{
				response->waveform = process_file(request->location, request->user_requested);
				request->completion_handler(response);
			});
		}
	}

	void cache_impl::remove_dead_waveforms()
	{
		if (store)
		{
			io.post([this]()
			{
				store->remove_dead();
			});
		}
	}

	void cache_impl::compact_storage()
	{
		if (store)
		{
			io.post([this]()
			{
				store->compact();
			});
		}
	}

	void cache_impl::rescan_waveforms()
	{
		if (store)
		{
			io.post([this]()
			{
				auto get_func = boost::bind(&cache_impl::get_waveform, this, _1);
				pfc::list_t<playable_location_impl> locations;
				store->get_all(locations);
				auto f = [get_func](playable_location const& loc)
				{
					auto req = boost::make_shared<get_request>();
					req->user_requested = true;
					req->location = loc;
					get_func(req);
				};
				locations.enumerate(f);
			});
		}
	}

	void cache_initquit::on_init()
	{}

	void cache_initquit::on_quit()
	{
		if (!core_api::is_quiet_mode_enabled())
		{
			try
			{
				static_api_ptr_t<cache> c;
				if (c.get_ptr())
					c->flush();
			}
			catch (std::exception&)
			{
			}
		}
	}
}