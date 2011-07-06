#pragma once
#include "PchSeekbar.h"

#include "Pack.h"

namespace pack { namespace detail {
	boost::thread_specific_ptr<encoder> enc;
} }