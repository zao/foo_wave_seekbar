//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include "Helpers.h"
#include <boost/format.hpp>
#include "Helpers.h"
#include <regex>

// {EBEABA3F-7A8E-4A54-A902-3DCF716E6A97}
const GUID guid_seekbar_branch = { 0xebeaba3f, 0x7a8e, 0x4a54, { 0xa9, 0x2, 0x3d, 0xcf, 0x71, 0x6e, 0x6a, 0x97 } };

// {1E01E2F7-79CE-4F3F-95FE-86986236670C}
static const GUID guid_max_concurrent_jobs = 
{ 0x1e01e2f7, 0x79ce, 0x4f3f, { 0x95, 0xfe, 0x86, 0x98, 0x62, 0x36, 0x67, 0xc } };

// {44AA5DAB-F35E-4E21-8033-80087B2550FD}
static const GUID guid_always_rescan_user = 
{ 0x44aa5dab, 0xf35e, 0x4e21, { 0x80, 0x33, 0x80, 0x8, 0x7b, 0x25, 0x50, 0xfd } };

static advconfig_integer_factory g_max_concurrent_jobs("Number of concurrent scanning threads (capped by virtual processor count)", guid_max_concurrent_jobs, guid_seekbar_branch, 0.0, 3, 1, 16);
static advconfig_checkbox_factory g_always_rescan_user("Always rescan track if requested by user", guid_always_rescan_user, guid_seekbar_branch, 0.0, false);

namespace wave
{
	cache_impl::cache_impl()
		: idle_work(new asio::io_service::work(io))
		, init_barrier(2)
	{
		is_initialized = false;
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

	void cache_impl::load_data(std::shared_ptr<boost::barrier> load_barrier)
	{
		open_store();

		std::deque<job> jobs;
		if (store)
		{
			store->get_jobs(jobs);
			for each (job j in jobs)
			{
				std::shared_ptr<get_request> request(new get_request);
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
			std::unique_lock<std::mutex> lk(cache_mutex);
			flush_callback.abort();
		}
		idle_work.reset();
		for (auto& t : work_threads) {
			t.join();
		}

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
		std::lock_guard<std::mutex> sl(init_mutex);
		init_barrier.wait();
		std::shared_ptr<boost::barrier> load_barrier(new boost::barrier(2));
		io.post([this, load_barrier]()
		{
			load_data(load_barrier);
		});

		size_t n_cores = boost::thread::hardware_concurrency();
		size_t n_cap = (size_t)g_max_concurrent_jobs.get();
		size_t n = std::min(n_cores, n_cap);

		for (size_t i = 0; i < n; ++i) {
			work_threads.emplace_back(with_idle_priority([this, i, n]()
			{
				std::string name = (boost::format("wave-processing-%d/%d") % (i+1) % n).str();
				::SetThreadName(-1, name.c_str());
				CoInitialize(nullptr);
				this->io.run();
				CoUninitialize();
			}));
			if (!i)
			{
				load_barrier->wait();
			}
		}
		is_initialized = true;
	}

	void cache_impl::try_delayed_init()
	{
		if (!is_initialized)
			std::lock_guard<std::mutex> sl(init_mutex);
	}

	void dispatch_partial_response(std::function<void (std::shared_ptr<get_response>)> completion_handler, ref_ptr<waveform> waveform, size_t buckets_filled)
	{
		auto response = std::make_shared<get_response>();
		response->waveform = waveform;
		response->valid_bucket_count = buckets_filled;
		completion_handler(response);
	}

	bool cache_impl::get_waveform_sync(playable_location const& loc, ref_ptr<waveform>& out)
	{
		try_delayed_init();
		if (has_waveform(loc))
			return store->get(out, loc);
		return false;
	}

	void cache_impl::get_waveform(std::shared_ptr<get_request> request)
	{
		if (std::regex_match(request->location.get_path(), std::regex("\\s*")))
			return;

		try_delayed_init();

		if (!store)
			return;

		bool force_rescan = g_always_rescan_user.get();
		bool should_rescan = force_rescan && request->user_requested || !store->has(request->location);

		auto response = std::make_shared<get_response>();
		if (!should_rescan)
		{
			store->get(response->waveform, request->location);
			request->completion_handler(response);
		}
		else
		{
			response->waveform = make_placeholder_waveform();
			response->valid_bucket_count = 0;
			request->completion_handler(response);
			
			response.reset(new get_response);
			if (!request->user_requested)
			{
				important_queue.push(request->location);
			}
			io.post([this, request, response]()
			{
				response->waveform = process_file(request->location, request->user_requested,
					std::make_shared<incremental_result_sink>(std::bind(&dispatch_partial_response, request->completion_handler, std::placeholders::_1, std::placeholders::_2)));
				request->completion_handler(response);
			});
		}
	}

	void cache_impl::remove_dead_waveforms()
	{
		try_delayed_init();
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
		try_delayed_init();
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
		try_delayed_init();
		if (store)
		{
			io.post([this]()
			{
				std::function<void (std::shared_ptr<get_request>)> get_func = std::bind(&cache_impl::get_waveform, this, std::placeholders::_1);
				pfc::list_t<playable_location_impl> locations;
				store->get_all(locations);
				auto f = [get_func](playable_location const& loc)
				{
					auto req = std::make_shared<get_request>();
					req->user_requested = true;
					req->location = loc;
					get_func(req);
				};
				locations.enumerate(f);
			});
		}
	}

	void cache_impl::compression_bench()
	{
		try_delayed_init();
		if (store)
		{
			io.post([this]()
			{
				store->bench();
			});
		}
	}

	void cache_impl::defer_action(std::function<void ()> fun)
	{
		try_delayed_init();
		io.post(fun);
	}

	bool cache_impl::is_location_forbidden(playable_location const& loc)
	{
		return is_of_forbidden_protocol(loc);
	}

	bool cache_impl::has_waveform(playable_location const& loc)
	{
		try_delayed_init();
		if (store)
		{
			return store->has(loc);
		}
		return false;
	}

	void cache_impl::remove_waveform(playable_location const& loc)
	{
		try_delayed_init();
		if (store)
		{
			store->remove(loc);
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
				c->flush();
			}
			catch (std::exception&)
			{
			}
		}
	}

	void cache_impl::kick_dynamic_init()
	{
		boost::thread(std::bind(&cache_impl::delayed_init, this)).detach();
		init_barrier.wait();
	}

	struct cache_init_stage : init_stage_callback
	{
		void on_init_stage(t_uint32 stage) override
		{
			if (!core_api::is_quiet_mode_enabled()) {
				if (stage == init_stages::before_config_read) {
					static_api_ptr_t<cache> c;
					auto* p = dynamic_cast<cache_impl*>(c.get_ptr());
					p->kick_dynamic_init();
				}
			}
		}
	};
}

static service_factory_single_t<wave::cache_init_stage> g_cache_init_stage;