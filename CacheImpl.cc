//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "CacheImpl.h"
#include "BackingStore.h"
#include "Helpers.h"
#include <regex>
#include <stdint.h>

#include <boost/atomic.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/thread.hpp>

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
	struct cache_run_state
	{
		cache_run_state()
			: init_sync(2)
		{}

		boost::thread* thread;
		boost::barrier init_sync;
		boost::mutex mutex;
		boost::condition_variable bump;
		boost::atomic<bool> should_shutdown;
	};
	static cache_run_state run_state;

	static std::deque<service_ptr_t<waveform_query> > requests_by_urgency[3];

	struct worker_result
	{
		playable_location_impl loc;
		ref_ptr<waveform> sig;
		uint16_t buckets_filled;
	};
	static std::vector<worker_result> worker_results;

	cache_impl::cache_impl()
	{
		is_initialized = 0;
	}

	cache_impl::~cache_impl()
	{
	}

	struct waveform_query_shared : waveform_query
	{
		waveform_query_shared()
			: progress(0), aborted(false)
		{}

		virtual playable_location const& get_location() const override { return loc; }
		virtual query_urgency get_urgency() const override { return urgency; }
		virtual query_force get_forced() const override { return forced; }
		virtual float get_progress() const override { return progress; }
		virtual ref_ptr<waveform> get_waveform() const { return wf; }
		virtual void abort() { aborted = true; }

	public:
		playable_location_impl loc;
		query_urgency urgency;
		query_force forced;
		float progress;
		ref_ptr<waveform> wf;
		bool aborted;
	};

	struct plain_query : waveform_query_shared
	{
		virtual void set_waveform(ref_ptr<waveform> wf, float progress) override
		{
			this->wf = wf;
		}
	};

	struct callback_query : waveform_query_shared
	{
		virtual void set_waveform(ref_ptr<waveform> wf, float progress) override
		{
			this->wf = wf;
			service_ptr_t<waveform_query> self;
			service_query_t(self);
			callback(self);
		}

		std::function<void(service_ptr_t<waveform_query>)> callback;
	};

	service_ptr_t<waveform_query> cache_impl::create_query(playable_location const& loc,
		waveform_query::query_urgency urgency, waveform_query::query_force forced)
	{
		service_ptr_t<plain_query> q = new service_impl_t<plain_query>();
		q->loc = loc;
		q->urgency = urgency;
		q->forced = forced;
		return q;
	}

	service_ptr_t<waveform_query> cache_impl::create_callback_query(playable_location const& loc,
		waveform_query::query_urgency urgency, waveform_query::query_force forced,
		std::function<void(service_ptr_t<waveform_query>)> callback)
	{
		service_ptr_t<callback_query> q = new service_impl_t<callback_query>();
		q->loc = loc;
		q->urgency = urgency;
		q->forced = forced;
		q->callback = callback;
		return q;
	}

	struct with_idle_priority
	{
		typedef std::function<void()> function_type;
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

	void cache_impl::load_data()
	{
		open_store();

		std::deque<job> jobs;
		if (store)
		{
			store->get_jobs(jobs);
			for (auto I = jobs.begin(); I != jobs.end(); ++I)
			{
				auto forced = I->user ? waveform_query::forced_query : waveform_query::unforced_query;
				auto q = create_query(I->loc, waveform_query::bulk_urgency, forced);
				get_waveform(q);
			}
		}
		else
		{
			console::warning("Wave cache: could not open backing database.");
		}
	}

	void cache_impl::flush()
	{
		// TODO(zao): Tear down workers, store actively queried jobs in DB
		/*
		{
		lock_guard<uv_mutex_t> lk(cache_mutex);
		flush_callback.abort();
		}
		io_work.reset();
		for (auto& t : work_threads) {
		uv_thread_join(&t);
		}
		work_post_queue.stop();
		uv_async_send(&work_dispatch_work);
		uv_thread_join(&work_dispatch_thread);

		open_store();
		if (store)
		{
		store->put_jobs(job_flush_queue);
		}

		store.reset();
		*/
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
		// TODO(zao): Spin up threads, prepare initial data load
		/*
		lock_guard<uv_mutex_t> lk(init_mutex);
		init_sync_point.set(true);

		uv_async_init(work_dispatch_loop, &work_dispatch_work, [](uv_async_t* handle, int status)
		{
		uv_close((uv_handle_t*)handle, nullptr);
		});

		work_post_queue.post([this]
		{
		load_data();
		});

		auto hardware_concurrency = []{
		SYSTEM_INFO info = {};
		GetSystemInfo(&info);
		return info.dwNumberOfProcessors;
		};

		size_t n_cores = hardware_concurrency();
		size_t n_cap = (size_t)g_max_concurrent_jobs.get();
		size_t n = std::min(n_cores, n_cap);

		io_work.reset(new asio::io_service::work(io));
		for (size_t i = 0; i < n; ++i) {
		work_threads.emplace_back();
		work_functions.emplace_back(with_idle_priority([this, i, n]
		{
		std::string name = "wave-processing-" + std::to_string(i+1) + "/" + std::to_string(n);
		::SetThreadName(-1, name.c_str());
		CoInitialize(nullptr);
		this->io.run();
		CoUninitialize();
		}));
		uv_thread_create(&work_threads.back(), [](void* f){ (*(std::function<void()>*)f)(); }, &work_functions.back());
		}
		is_initialized = 1;
		*/
	}

	void cache_impl::try_delayed_init()
	{
		// TODO(zao): Wait for pending initialization
		/*
		if (!is_initialized) {
		lock_guard<uv_mutex_t> lk(init_mutex);
		}
		*/
	}

	void dispatch_partial_response(std::function<void(std::shared_ptr<get_response>)> completion_handler, ref_ptr<waveform> waveform, size_t buckets_filled)
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

	void cache_impl::get_waveform(service_ptr_t<waveform_query> request)
	{
		auto& loc = request->get_location();
		if (std::regex_match(loc.get_path(), std::regex("\\s*")))
			return;

		try_delayed_init();

		if (!store)
			return;

		bool force_rescan = g_always_rescan_user.get();
		bool should_rescan = request->get_forced() || !store->has(loc);

		auto response = std::make_shared<get_response>();
		if (!should_rescan)
		{
			ref_ptr<waveform> wf;
			store->get(wf, loc);
			request->set_waveform(wf, 2048);
			// TODO(zao): Return responses for callbacks
			/*
			request->completion_handler(response);
			*/
		}
		else
		{
			boost::unique_lock<boost::mutex> lk(worker_mutex);
			switch (request->get_urgency()) {
			case waveform_query::needed_urgency: {
				requests_by_urgency[waveform_query::needed_urgency].push_front(request);
			} break;
			case waveform_query::desired_urgency: {
				requests_by_urgency[waveform_query::desired_urgency].push_back(request);
			} break;
			case waveform_query::bulk_urgency: {
				requests_by_urgency[waveform_query::bulk_urgency].push_back(request);
			} break;
			}
			worker_bump.notify_one();
		}
	}

	void cache_impl::remove_dead_waveforms()
	{
		try_delayed_init();
		if (store)
		{
			defer_action([this]{
				store->remove_dead();
			});
		}
	}

	void cache_impl::compact_storage()
	{
		try_delayed_init();
		if (store)
		{
			defer_action([this]{
				store->compact();
			});
		}
	}

	void cache_impl::rescan_waveforms()
	{
		try_delayed_init();
		if (store)
		{
			defer_action([this]{
				pfc::list_t<playable_location_impl> locations;
				store->get_all(locations);
				for (size_t i = 0; i < locations.get_size(); ++i) {
					auto q = create_query(locations[i], waveform_query::bulk_urgency, waveform_query::forced_query);
					get_waveform(q);
				}
			});
		}
	}

	void cache_impl::defer_action(std::function<void()> fun)
	{
		try_delayed_init();
		// TODO(zao): Run maintenance task off-thread. Do these in cache_main?
		fun();
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
				auto* p = dynamic_cast<cache_impl*>(c.get_ptr());
				p->shutdown();
			}
			catch (std::exception&)
			{
			}
		}
	}

	void cache_impl::start()
	{
		OutputDebugStringA("Starting cache.\n");
		run_state.thread = new boost::thread(boost::bind(&cache_impl::cache_main, this));
		run_state.init_sync.wait();
	}

	void cache_impl::shutdown()
	{
		OutputDebugStringA("Stopping cache.\n");
		{
			boost::unique_lock<boost::mutex> lk(cache_mutex);
			run_state.should_shutdown = true;
			run_state.bump.notify_one();
		}
		run_state.thread->join();
		delete run_state.thread;
	}

	void cache_impl::worker_main(size_t i, size_t n)
	{
		char name_buf[128] = {};
		sprintf_s(name_buf, "wave-processing-%d/%d", i+1, n);
		::SetThreadName(-1, name_buf);
		CoInitialize(nullptr);
		auto is_ready = [&]() -> bool { return should_workers_terminate; };
		while (1) {
			playable_location_impl requested_loc;
			{
				boost::unique_lock<boost::mutex> lk(worker_mutex);
				worker_bump.wait(lk, is_ready);
				if (should_workers_terminate) {
					break;
				}
			}
			// TODO(zao): Prepare and use priority-bucketed in-progress state.
			// TODO(zao): Turn process_file use into an incremental pre-emptible process.
			// TODO(zao): Get hold of user-requestedness


			/*
			worker_result out;
			out.loc = requested_loc;
			out.sig = process_file(requested_loc, true);
			for (int i = 1; i <= 4; ++i) {
				out.buckets_filled = i*(2048/4);
				{
					boost::unique_lock<boost::mutex> lk(worker_mutex);
					worker_results.push_back(out);
					run_state.bump.notify_one();
				}
			}
			*/
		}
		CoUninitialize();
	}

	void cache_impl::cache_main()
	{
		run_state.init_sync.wait();

		// TODO(zao): Should data loading be in this thread?
		load_data();
		std::vector<boost::thread*> worker_threads;

		size_t n_cores = boost::thread::hardware_concurrency();
		size_t n_cap = (size_t)g_max_concurrent_jobs.get();
		size_t n = std::min(n_cores, n_cap);

		for (size_t i = 0; i < n; ++i) {
			boost::thread* t = new boost::thread(with_idle_priority(boost::bind(&cache_impl::worker_main, this, i, n)));
			worker_threads.push_back(t);
		}

		OutputDebugStringA("Cache ready.\n");
		boost::unique_lock<boost::mutex> lk(run_state.mutex);
		auto is_ready = [&]() -> bool { return run_state.should_shutdown || worker_results.size(); };
		while (1) {
			run_state.bump.wait(lk, is_ready);
			if (run_state.should_shutdown) {
				break;
			}
			for (auto I = worker_results.begin(); I != worker_results.end(); ++I) {
				if (I->buckets_filled == 2048) {
					OutputDebugStringA("Got a final result from worker.\n");
				}
				else {
					char buf[128] = {};
					sprintf_s(buf, "Got a partial result from worker, %d/2048 buckets.\n", I->buckets_filled);
					OutputDebugStringA(buf);
				}
			}
			worker_results.clear();
		}
	}

	struct cache_init_stage : init_stage_callback
	{
		void on_init_stage(t_uint32 stage) override
		{
			if (!core_api::is_quiet_mode_enabled()) {
				if (stage == init_stages::before_config_read) {
					static_api_ptr_t<cache> c;
					auto* p = dynamic_cast<cache_impl*>(c.get_ptr());
					p->start();
				}
			}
		}
	};
}

static service_factory_single_t<wave::cache_init_stage> g_cache_init_stage;