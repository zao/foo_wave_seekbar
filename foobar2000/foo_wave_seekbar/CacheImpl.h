//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "Cache.h"
#include "waveform_sdk/Waveform.h"
#include "Job.h"
#include <list>
#include <stack>
#include <intrin.h>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>


// {EBEABA3F-7A8E-4A54-A902-3DCF716E6A97}
extern const GUID guid_seekbar_branch;

namespace wave
{
	inline bool LocationLessThan (const playable_location_impl& x, const playable_location_impl& y)
	{
		int cmp = strcmp(x.get_path(), y.get_path());
		if (cmp == 0)
			return x.get_subsong() < y.get_subsong();
		return cmp < 0;
	}

	struct backing_store;

	bool is_of_forbidden_protocol(playable_location const& loc);

	namespace process_result
	{
		enum type
		{
			failed,
			aborted,
			elided,
			not_done,
			done,
		};
	};

	struct process_state;

	struct cache_impl : cache
	{
		cache_impl();
		~cache_impl();

		virtual service_ptr_t<waveform_query> create_query(playable_location const& loc,
			waveform_query::query_urgency urgency, waveform_query::query_force forced) override;
		virtual service_ptr_t<waveform_query> create_callback_query(playable_location const& loc,
			waveform_query::query_urgency urgency, waveform_query::query_force forced,
			std::function<void(service_ptr_t<waveform_query>)> callback) override;

		void get_waveform(service_ptr_t<waveform_query> query) override;
		void remove_dead_waveforms() override;
		void compact_storage() override;
		void rescan_waveforms() override;

		bool has_waveform(playable_location const& loc) override;
		void remove_waveform(playable_location const& loc) override;

		void defer_action(std::function<void ()> fun) override;

		bool is_location_forbidden(playable_location const& loc) override;
		bool get_waveform_sync(playable_location const& loc, ref_ptr<waveform>& out) override;

		typedef std::function<void (ref_ptr<waveform>, size_t)> incremental_result_sink;

		void start();
		void shutdown();

	private:
		void cache_main();
		void worker_main(size_t i, size_t n);
		void open_store();
		void load_data();
		process_result::type process_file(service_ptr_t<waveform_query> q, std::shared_ptr<process_state>& state);
		float render_progress(process_state* state);
		ref_ptr<waveform> render_waveform(process_state* state);
		bool is_refresh_due(process_state* state);

		std::atomic<bool> should_workers_terminate;
		std::mutex worker_mutex;
		std::condition_variable worker_bump;

		std::atomic<long> is_initialized;
		std::mutex init_mutex;
		std::future<bool> init_sync_point;

		std::mutex important_mutex;
		std::stack<playable_location_impl> important_queue;

		pfc::string cache_filename;
		std::mutex cache_mutex;
		std::list<std::thread*> work_threads;
		std::list<std::function<void()>> work_functions;
		typedef bool (*playable_compare_pointer)(const playable_location_impl&, const playable_location_impl&);
		abort_callback_impl flush_callback;
		std::deque<service_ptr_t<waveform_query> > job_flush_queue;
		std::shared_ptr<backing_store> store;
	};

	struct cache_initquit : initquit
	{
		void on_init();
		void on_quit();
	};

	bool try_determine_song_parameters(service_ptr_t<input_decoder>& decoder, t_uint32 subsong,
		t_int64& sample_rate, t_int64& sample_count, abort_callback& abort_cb);
}
