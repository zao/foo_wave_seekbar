//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "Job.h"
#include "waveform_sdk/Waveform.h"

namespace wave {
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

    void get_jobs(std::deque<job>&);
    void put_jobs(std::deque<job> const&);

    void get_all(pfc::list_t<playable_location_impl>&);

  private:
    std::shared_ptr<sqlite3_stmt> prepare_statement(std::string const& query);
    std::shared_ptr<sqlite3> backing_db;
};
}
