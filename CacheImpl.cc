#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include "Helpers.h"
#include <boost/format.hpp>
#include "Helpers.h"

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
		size_t n_cap = (size_t)g_max_concurrent_jobs.get();
		size_t n = std::min(n_cores, n_cap);

		for (size_t i = 0; i < n; ++i) {
			work_threads.create_thread(with_idle_priority([this, i, n]()
			{
				std::string name = (boost::format("wave-processing-%d/%d") % (i+1) % n).str();
				::SetThreadName(-1, name.c_str());
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
		bool force_rescan = g_always_rescan_user.get();
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
				static_api_ptr_t<metadb> mdb;
				metadb_handle_ptr handle;
				mdb->handle_create(handle, request->location);

				response->waveform = process_file(request->location, request->user_requested);

				in_main_thread([handle]
				{
					static_api_ptr_t<metadb_io> io;
					// for notification to users of titleformatting hooks
					io->dispatch_refresh(handle);
				});

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

	bool cache_impl::has_waveform(playable_location const& loc)
	{
		if (store)
		{
			return store->has(loc);
		}
		return false;
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