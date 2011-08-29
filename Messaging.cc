#include "PchSeekbar.h"
#include "Messaging.h"

static zmq::context_t* ctx = NULL;

namespace messaging
{
	struct zmq_iq : initquit
	{
		virtual void on_quit() 
		{
			delete ::ctx;
		}

		virtual void on_init() 
		{
			::ctx = new zmq::context_t(1);
		}
	};

	zmq::context_t& ctx()
	{
		return *::ctx;
	}
}

static initquit_factory_t<messaging::zmq_iq> g_asdf;