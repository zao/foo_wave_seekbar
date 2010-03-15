#include "PchSeekbar.h"
#include "BackingStore.h"
#include "WaveformImpl.h"
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/zlib.hpp>

namespace wave
{
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

		sqlite3_exec(
			backing_db.get(),
			"ALTER TABLE wave ADD channels INT",
			0, 0, 0);

		sqlite3_exec(
			backing_db.get(),
			"ALTER TABLE wave ADD compression INT",
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
			"SELECT w.min, w.max, w.rms, w.channels, w.compression "
			"FROM file AS f NATURAL JOIN wave AS w "
			"WHERE f.location = ? AND f.subsong = ?");

		sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 2, file.get_subsong());

		if (SQLITE_ROW != sqlite3_step(stmt.get())) {
			return false;
		}
		
		boost::optional<int> channels, compression;

		if (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL)
			channels = sqlite3_column_int(stmt.get(), 3);
		if (sqlite3_column_type(stmt.get(), 4) != SQLITE_NULL)
			compression = sqlite3_column_int(stmt.get(), 4);

		if (compression && *compression > 0 || channels)
			return false;

		service_ptr_t<waveform_impl> w = new service_impl_t<waveform_impl>;
		auto clear_and_set = [&stmt, compression](decltype(w->rms)& list, size_t col)
		{
			list.remove_all();
				
			float const* data = (float const*)sqlite3_column_blob(stmt.get(), col);
			t_size count = sqlite3_column_bytes(stmt.get(), col) / sizeof(float);

			if (compression)
			{
				std::vector<float> dst(2048);
				if (*compression == 0)
				{
					io::filtering_ostream out;
					out.push(io::zlib_decompressor());
					out.push(io::array_sink((char*)&dst[0], dst.size() * sizeof(float)));
					out.write((char const*)data, count * sizeof(float));
				}
				list.add_items_fromptr(&dst[0], dst.size());
			}
			else
			{
				list.add_items_fromptr(data, count);
			}

		};

		clear_and_set(w->minimum, 0);
		clear_and_set(w->maximum, 1);
		clear_and_set(w->rms, 2);

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
			"REPLACE INTO wave (fid, min, max, rms, channels, compression) "
			"SELECT f.fid, ?, ?, ?, NULL, 0 "
			"FROM file AS f "
			"WHERE f.location = ? AND f.subsong = ?");

#		define BIND_LIST(Member, Idx) \
			pfc::list_t<float> Member; \
			w->get_field(#Member, Member); \
			std::vector<char> Member ## _buf; \
			{ \
				io::filtering_ostream out( \
					io::zlib_compressor(9) | \
					io::back_inserter(Member ## _buf)); \
				out.write((char*)Member.get_ptr(), Member.get_size() * sizeof(float)); \
			} \
			sqlite3_bind_blob(stmt.get(), Idx, &(Member ## _buf)[0], (Member ## _buf).size(), SQLITE_STATIC)
			
		BIND_LIST(minimum, 1);
		BIND_LIST(maximum, 2);
		BIND_LIST(rms    , 3);

#		undef  BIND_LIST

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