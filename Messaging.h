#pragma once
#include <zmq.hpp>

namespace messaging
{
	zmq::context_t& ctx();
}