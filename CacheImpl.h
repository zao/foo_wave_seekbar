//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "Cache.h"
#include "waveform_sdk/Waveform.h"
#include "Job.h"

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

	struct cache_impl : cache_v4
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
		
		virtual bool get_waveform_info(playable_location const& loc, waveform_info& out) override;

	private:
		void open_store();
		void load_data(shared_ptr<boost::barrier>);
		void delayed_init();
		service_ptr_t<waveform> process_file(playable_location_impl loc, bool user_requested);

		boost::mutex important_mutex;
		std::stack<playable_location_impl> important_queue;

		pfc::string cache_filename;
		boost::mutex cache_mutex;
		boost::asio::io_service io;
		scoped_ptr<boost::asio::io_service::work> idle_work;
		boost::thread_group work_threads;
		long initialized;
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
}
