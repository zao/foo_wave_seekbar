//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "BackingStore.h"
#include "waveform_sdk/WaveformImpl.h"
#include "Helpers.h"
#include "Pack.h"
#include "waveform_sdk/Optional.h"

#include "sqlite3.h"

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
		auto stmt = prepare_statement(
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

	void backing_store::remove(playable_location const& file)
	{
		auto stmt = prepare_statement(
			"DELETE FROM file WHERE file.location = ? AND file.subsong = ?");
		sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 2, file.get_subsong());
		sqlite3_step(stmt.get());
	}

	bool backing_store::get(ref_ptr<waveform>& out, playable_location const& file)
	{
		out.reset();
		wave::optional<int> compression;
		{
			auto stmt = prepare_statement(
				"SELECT w.min, w.max, w.rms, w.channels, w.compression "
				"FROM file AS f NATURAL JOIN wave AS w "
				"WHERE f.location = ? AND f.subsong = ?");

			sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt.get(), 2, file.get_subsong());

			if (SQLITE_ROW != sqlite3_step(stmt.get())) {
				return false;
			}
		
			wave::optional<int> channels;

			if (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL)
				channels = sqlite3_column_int(stmt.get(), 3);
			if (sqlite3_column_type(stmt.get(), 4) != SQLITE_NULL)
				compression = sqlite3_column_int(stmt.get(), 4);

			if (compression && *compression > 1)
				return false;

			unsigned channel_count = channels ? count_bits_set(*channels) : 1;

			if (compression && *compression < 0 || channels && *channels < 0 || channel_count > 18) {
				remove(file); // corrupt entry
			}

			ref_ptr<waveform_impl> w(new waveform_impl);
			auto clear_and_set = [&stmt, compression, channel_count, &w](pfc::string name, size_t col) -> bool
			{
				pfc::list_t<waveform_impl::signal> list;
			
				float const* data = (float const*)sqlite3_column_blob(stmt.get(), col);
				t_size count = sqlite3_column_bytes(stmt.get(), col);

				if (compression)
				{
					typedef std::back_insert_iterator<std::vector<char>> Iterator;
					bool (*unpack_func)(void const*, size_t, Iterator) = 0;
					switch (*compression) {
					case 0: unpack_func = &pack::z_unpack<Iterator>; break;
					case 1: unpack_func = &pack::lzma_unpack<Iterator>; break;
					default: return false; // unknown compression scheme
					}

					std::vector<char> dst;
					dst.reserve(2048 * channel_count * sizeof(float));
					if (!unpack_func(data, count, std::back_inserter(dst)))
					{
						return false;
					}

					if (dst.size() != channel_count * 2048 * sizeof(float))
					{
						return false;
					}

					for (unsigned c = 0; c < channel_count; ++c)
					{
						waveform_impl::signal channel;
						float const * fs = (float*)&dst[2048 * c * sizeof(float)];
						channel.add_items_fromptr(fs, 2048);
						list.add_item(channel);
					}
				}
				else
				{
					for (unsigned c = 0; c < channel_count; ++c)
					{
						waveform_impl::signal channel;
						channel.add_items_fromptr(data + 2048*c, 2048);
						list.add_item(channel);
					}
				}
				w->fields[name] = list;
				return true;
			};

			if (clear_and_set("minimum", 0) && 
				clear_and_set("maximum", 1) &&
				clear_and_set("rms", 2))
			{
				w->channel_map = channels ? *channels : audio_chunk::channel_config_mono;

				out = w;
			}
			else
			{
				remove(file); // it's corrupt, and thus useless
			}
		}

		if(!compression || *compression == 0)
		{
			put(out, file);
		}
		return out.is_valid();
	}

	void backing_store::put(ref_ptr<waveform> const& w, playable_location const& file)
	{
		auto stmt = prepare_statement(
			"INSERT INTO file (location, subsong) "
			"VALUES (?, ?)");
		sqlite3_bind_text(stmt.get(), 1, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 2, file.get_subsong());
		sqlite3_step(stmt.get());

		stmt = prepare_statement(
			"REPLACE INTO wave (fid, min, max, rms, channels, compression) "
			"SELECT f.fid, ?, ?, ?, ?, ? "
			"FROM file AS f "
			"WHERE f.location = ? AND f.subsong = ?");

#		define BIND_LIST(Member, Idx) \
			std::vector<char> Member; \
			{ \
				std::vector<float> src_buf; \
				for (size_t c = 0; c < w->get_channel_count(); ++c) \
				{ \
					pfc::list_t<float> channel; \
					w->get_field(#Member, c, list_array_sink<float>(channel)); \
					float * p = (float *)channel.get_ptr(); \
					std::copy(p, p + channel.get_size(), std::back_inserter(src_buf)); \
				} \
				pack::lzma_pack(&src_buf[0], src_buf.size() * sizeof(float), std::back_inserter(Member)); \
			} \
			sqlite3_bind_blob(stmt.get(), Idx, &Member[0], Member.size(), SQLITE_STATIC)
			
		BIND_LIST(minimum, 1);
		BIND_LIST(maximum, 2);
		BIND_LIST(rms    , 3);

#		undef  BIND_LIST

		sqlite3_bind_int(stmt.get(), 4, w->get_channel_map());
		sqlite3_bind_int(stmt.get(), 5, 1); // LZMA compression
		sqlite3_bind_text(stmt.get(), 6, file.get_path(), -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt.get(), 7, file.get_subsong());

		while (SQLITE_ROW == sqlite3_step(stmt.get()));
	}

	void backing_store::get_jobs(std::deque<job>& out)
	{
		auto stmt = prepare_statement(
			"SELECT location, subsong, user_submitted FROM job ORDER BY jid");

		out.clear();
		while (SQLITE_ROW == sqlite3_step(stmt.get()))
		{
			char const* loc = (char const*)sqlite3_column_text(stmt.get(), 0);
			t_uint32 sub = (t_uint32)sqlite3_column_int(stmt.get(), 1);
			bool user = !!sqlite3_column_int(stmt.get(), 2);
			out.push_back(make_job(playable_location_impl(loc, sub), user));
		}
	}

	void backing_store::put_jobs(std::deque<job> const& jobs)
	{
		sqlite3_exec(backing_db.get(), "BEGIN", 0, 0, 0);
		sqlite3_exec(backing_db.get(), "DELETE FROM job", 0, 0, 0);
		auto stmt = prepare_statement(
			"INSERT INTO job (location, subsong, user_submitted) "
			"VALUES (?, ?, ?)");

		std::deque<job>::const_iterator from = jobs.begin(), to = jobs.end();
		for (; from != to; ++from) {
			job const& j = *from;
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

	void backing_store::get_all(pfc::list_t<playable_location_impl>& out)
	{
		auto stmt = prepare_statement(
			"SELECT location, subsong FROM file ORDER BY location, subsong");

		out.remove_all();
		while (SQLITE_ROW == sqlite3_step(stmt.get()))
		{
			char const* loc = (char const*)sqlite3_column_text(stmt.get(), 0);
			t_uint32 sub = (t_uint32)sqlite3_column_int(stmt.get(), 1);
			out.add_item(playable_location_impl(loc, sub));
		}
	}

	std::shared_ptr<sqlite3_stmt> backing_store::prepare_statement(std::string const& query)
	{
		sqlite3_stmt* p = 0;
		sqlite3_prepare_v2(
			backing_db.get(),
			query.c_str(),
			query.size(), &p, 0);
		return std::shared_ptr<sqlite3_stmt>(p, &sqlite3_finalize);
	}
}
