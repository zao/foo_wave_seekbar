#pragma once

#include "Cache.h"

namespace wave
{
	inline bool LocationLessThan (const playable_location_impl& x, const playable_location_impl& y)
	{
		int cmp = strcmp(x.get_path(), y.get_path());
		if (cmp == 0)
			return x.get_subsong() < y.get_subsong();
		return cmp < 0;
	}

	struct job
	{
		playable_location_impl loc;
		bool user;
	};

	inline job make_job(playable_location_impl loc, bool user)
	{
		job ret = { loc, user };
		return ret;
	}

	struct backing_store
	{
		explicit backing_store(pfc::string const& cache_filename);
		~backing_store();

		bool has(playable_location const& file);
		bool get(service_ptr_t<waveform>& out, playable_location const& file);
		void put(service_ptr_t<waveform> const& in, playable_location const& file);
		void remove_dead();
		void compact();

		void get_jobs(std::deque<job>&);
		void put_jobs(std::deque<job> const&);

	private:
		shared_ptr<sqlite3_stmt> prepare_statement(std::string const& query);
		shared_ptr<sqlite3> backing_db;
	};

	struct waveform_impl : waveform
	{
		virtual bool get_field(pfc::string const& what, pfc::list_base_t<float>& out) override;
		pfc::list_t<float> minimum, maximum, rms;
	};

	struct cache_impl : cache
	{
		cache_impl();
		~cache_impl();

		void get_waveform(shared_ptr<get_request>) override;
		void remove_dead_waveforms() override;
		void compact_storage() override;

		void flush() override;

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