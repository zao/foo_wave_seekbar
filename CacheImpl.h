//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "util/Asio.h"
#include "Cache.h"
#include "waveform_sdk/Waveform.h"
#include "Job.h"
#include <boost/thread.hpp>
#include <stack>

// {EBEABA3F-7A8E-4A54-A902-3DCF716E6A97}
extern const GUID guid_seekbar_branch;

namespace wave
{
	using boost::scoped_ptr;
	inline bool LocationLessThan (const playable_location_impl& x, const playable_location_impl& y)
	{
		int cmp = strcmp(x.get_path(), y.get_path());
		if (cmp == 0)
			return x.get_subsong() < y.get_subsong();
		return cmp < 0;
	}

	struct backing_store;

	bool is_of_forbidden_protocol(playable_location const& loc);

	struct cache_impl : cache_v5
	{
		cache_impl();
		~cache_impl();

		void get_waveform(shared_ptr<get_request>) override;
		void remove_dead_waveforms() override;
		void compact_storage() override;
		void rescan_waveforms() override;

		void flush() override;

		bool has_waveform(playable_location const& loc) override;
		void remove_waveform(playable_location const& loc) override;

		void compression_bench() override;

		void defer_action(boost::function<void ()> fun) override;

		bool is_location_forbidden(playable_location const& loc) override;
		bool get_waveform_sync(playable_location const& loc, ref_ptr<waveform>& out) override;

		typedef boost::function<void (ref_ptr<waveform>, size_t)> incremental_result_sink;

		void kick_dynamic_init();

	private:
		void open_store();
		void load_data(shared_ptr<boost::barrier>);
		void try_delayed_init();
		void delayed_init();
		ref_ptr<waveform> process_file(playable_location_impl loc, bool user_requested, boost::shared_ptr<incremental_result_sink> incremental_output = boost::shared_ptr<incremental_result_sink>());

		tbb::atomic<bool> is_initialized;
		boost::barrier init_barrier;
		boost::mutex init_mutex;

		boost::mutex important_mutex;
		std::stack<playable_location_impl> important_queue;

		pfc::string cache_filename;
		boost::mutex cache_mutex;
		boost::asio::io_service io;
		scoped_ptr<boost::asio::io_service::work> idle_work;
		boost::thread_group work_threads;
		typedef bool (*playable_compare_pointer)(const playable_location_impl&, const playable_location_impl&);
		abort_callback_impl flush_callback;
		std::deque<job> job_flush_queue;
		//typedef std::map<playable_location_impl, waveform, playable_compare_pointer> waveform_map;
		//waveform_map waveforms;
		shared_ptr<backing_store> store;
	};

	struct cache_initquit : initquit
	{
		void on_init();
		void on_quit();
	};

	bool try_determine_song_parameters(service_ptr_t<input_decoder>& decoder, t_uint32 subsong,
		t_int64& sample_rate, t_int64& sample_count, abort_callback& abort_cb);
}
