#include "PchSeekbar.h"
#include "CacheImpl.h"
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

	bool waveform_impl::get_field(pfc::string const& what, pfc::list_base_t<float>& out)
	{
		if (0 == pfc::string::g_compare(what, "minimum"))
		{
			out = minimum;
			return true;
		}
		if (0 == pfc::string::g_compare(what, "maximum"))
		{
			out = maximum;
			return true;
		}
		if (0 == pfc::string::g_compare(what, "rms"))
		{
			out = rms;
			return true;
		}
		return false;
	}

	backing_store::backing_store(pfc::string const& cache_filename)
	{
		{
			sqlite3* p = 0;
			sqlite3_open(cache_filename.get_ptr(), &p);
			backing_db.reset(p, &sqlite3_close);
		}

		sqlite3_exec(
			backing_db.get(),
			"PRAGMA foreign_keys = ON",
			0, 0, 0);

		sqlite3_exec(
			backing_db.get(),
			"CREATE TABLE IF NOT EXISTS file ("
			"fid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
			"location TEXT NOT NULL,"
			"subsong INTEGER NOT NULL,"
			"UNIQUE (location, subsong))",
			0, 0, 0);

		sqlite3_exec(
			backing_db.get(),
			"CREATE TABLE IF NOT EXISTS wave ("
			"fid INTEGER PRIMARY KEY NOT NULL,"
			"min BLOB,"
			"max BLOB,"
			"rms BLOB,"
			"FOREIGN KEY (fid) REFERENCES file(fid))",
			0, 0, 0);

		sqlite3_exec(
			backing_db.get(),
			"CREATE TABLE IF NOT EXISTS job ("
			"jid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
			"location TEXT NOT NULL,"
			"subsong INTEGER NOT NULL,"
			"user_submitted INTEGER,"
			"UNIQUE (location, subsong))",
			0, 0, 0);
		
		sqlite3_exec(
			backing_db.get(),
			"DROP TRIGGER resonance_cascade",
			0, 0, 0);

		sqlite3_exec(
			backing_db.get(),
			"CREATE TRIGGER resonance_cascade BEFORE DELETE ON file BEGIN DELETE FROM wave WHERE wave.fid = OLD.fid; END",
			0, 0, 0);
	}

	backing_store::~backing_store()
	{
	}

	bool backing_store::has(playable_location const& file)
	{
		shared_ptr<sqlite3_stmt> stmt = prepare_statement(
			"SELECT 1 "
			"FROM file as f, wave AS w "
			"WHERE f.location = ? AND f.subsong = ? AND f.fid = w.fid");

		sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 2, file.get_subsong());

		if (SQLITE_ROW == sqlite3_step(stmt.get())) {
			return true;
		}
		return false;
	}

	bool backing_store::get(service_ptr_t<waveform>& out, playable_location const& file)
	{
		shared_ptr<sqlite3_stmt> stmt = prepare_statement(
			"SELECT w.min, w.max, w.rms "
			"FROM file AS f NATURAL JOIN wave AS w "
			"WHERE f.location = ? AND f.subsong = ?");

		sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 2, file.get_subsong());

		if (SQLITE_ROW != sqlite3_step(stmt.get())) {
			return false;
		}

#define CLEAR_AND_SET(Member, Col) w->Member.remove_all(); w->Member.add_items_fromptr((float const*)sqlite3_column_blob(stmt.get(), Col), sqlite3_column_bytes(stmt.get(), Col) / sizeof(float))

		service_ptr_t<waveform_impl> w = new service_impl_t<waveform_impl>;
		CLEAR_AND_SET(minimum, 0);
		CLEAR_AND_SET(maximum, 1);
		CLEAR_AND_SET(rms, 2);

#undef  CLEAR_AND_SET

		out = w;
		return true;
	}

	void backing_store::put(service_ptr_t<waveform> const& w, playable_location const& file)
	{
		shared_ptr<sqlite3_stmt> stmt;
		stmt = prepare_statement(
			"INSERT INTO file (location, subsong) "
			"VALUES (?, ?)");
		sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 2, file.get_subsong());
		sqlite3_step(stmt.get());

		stmt = prepare_statement(
			"REPLACE INTO wave (fid, min, max, rms) "
			"SELECT f.fid, ?, ?, ? "
			"FROM file AS f "
			"WHERE f.location = ? AND f.subsong = ?");

#define BIND_LIST(Member, Idx) pfc::list_t<float> Member; w->get_field(#Member, Member); sqlite3_bind_blob(stmt.get(), Idx, Member.get_ptr(), Member.get_size() * sizeof(float), SQLITE_STATIC)

		BIND_LIST(minimum, 1);
		BIND_LIST(maximum, 2);
		BIND_LIST(rms    , 3);

#undef  BIND_LIST

		sqlite3_bind_text(stmt.get(), 4, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 5, file.get_subsong());

		while (SQLITE_ROW == sqlite3_step(stmt.get()))
		{
		}
	}

	void backing_store::get_jobs(std::deque<job>& out)
	{
		shared_ptr<sqlite3_stmt> stmt = prepare_statement(
			"SELECT location, subsong, user_submitted FROM job ORDER BY jid");

		out.clear();
		while (SQLITE_ROW == sqlite3_step(stmt.get()))
		{
			char const* loc = (char const*)sqlite3_column_text(stmt.get(), 0);
			t_uint32 sub = (t_uint32)sqlite3_column_int(stmt.get(), 1);
			bool user = !!sqlite3_column_int(stmt.get(), 2);
			out += make_job(playable_location_impl(loc, sub), user);
		}
	}

	void backing_store::put_jobs(std::deque<job> const& jobs)
	{
		sqlite3_exec(backing_db.get(), "BEGIN", 0, 0, 0);
		sqlite3_exec(backing_db.get(), "DELETE FROM job", 0, 0, 0);
		shared_ptr<sqlite3_stmt> stmt = prepare_statement(
			"INSERT INTO job (location, subsong, user_submitted) "
			"VALUES (?, ?, ?)");

		BOOST_FOREACH(job j, jobs)
		{
			sqlite3_bind_text(stmt.get(), 1, j.loc.get_path(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt.get(), 2, j.loc.get_subsong());
			sqlite3_bind_int(stmt.get(), 3, j.user);
			sqlite3_step(stmt.get());
			sqlite3_reset(stmt.get());
		}
		sqlite3_exec(backing_db.get(), "COMMIT", 0, 0, 0);
	}

	void file_exists(sqlite3_context* ctx, int argc, sqlite3_value** argv)
	{
		char const* loc = (char const*)sqlite3_value_text(argv[0]);
		abort_callback_dummy cb;
		try {
			bool exists = filesystem::g_exists(loc, cb);

			if (exists)
			{
				sqlite3_result_int(ctx, exists);
				return;
			}
		}
		catch (exception_io&) {}
		sqlite3_result_null(ctx);
	}

	void backing_store::remove_dead()
	{
		sqlite3_create_function(
			backing_db.get(),
			"file_exists", 1, SQLITE_UTF8, 0,
			&file_exists, 0, 0);
		sqlite3_exec(backing_db.get(), "DELETE FROM file WHERE file_exists(file.location) IS NULL", 0, 0, 0);
		console::info("Waveform cache: removed dead entries from the database.");
	}

	void backing_store::compact()
	{
		sqlite3_exec(backing_db.get(), "VACUUM", 0, 0, 0);
		console::info("Waveform cache: compacted the database.");
	}

	shared_ptr<sqlite3_stmt> backing_store::prepare_statement(std::string const& query)
	{
		sqlite3_stmt* p = 0;
		sqlite3_prepare_v2(
			backing_db.get(),
			query.c_str(),
			query.size(), &p, 0);
		return shared_ptr<sqlite3_stmt>(p, &sqlite3_finalize);
	}
}