#pragma once
#include "../foo_wave_cache/Cache.h"

namespace wave
{
	struct seekbar_state
	{
		seekbar_state();

		double last_attempt_time;
		bool data_is_current;
	};
}