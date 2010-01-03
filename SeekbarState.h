#pragma once
#include "Cache.h"

namespace wave
{
	struct seekbar_state
	{
		seekbar_state();

		double last_attempt_time;
		bool data_is_current;
	};
}