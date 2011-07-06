//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace wave
{
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
}
