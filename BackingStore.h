//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "Job.h"
#include "waveform_sdk/Waveform.h"

struct sqlite3;
namespace std
{
	template <>
	struct default_delete<sqlite3>
	{
		void operator () (sqlite3* p) const { sqlite3_close(p); }
	};

	template <>
	struct default_delete<sqlite3_stmt>
	{
		void operator () (sqlite3_stmt* p) const { sqlite3_finalize(p); }
	};
}

namespace wave
{
	struct backing_store
	{
		explicit backing_store(pfc::string const& cache_filename);
		~backing_store();

		bool has(playable_location const& file);
		void remove(playable_location const& file);
		bool get(ref_ptr<waveform>& out, playable_location const& file);
		void put(ref_ptr<waveform> const& in, playable_location const& file);
		void remove_dead();
		void compact();
		void bench();

		void get_jobs(std::deque<job>&);
		void put_jobs(std::deque<job> const&);

		void get_all(pfc::list_t<playable_location_impl>&);

	private:
		std::unique_ptr<sqlite3_stmt> prepare_statement(std::string const& query);
		std::unique_ptr<sqlite3> backing_db;
	};
}
