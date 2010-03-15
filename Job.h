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