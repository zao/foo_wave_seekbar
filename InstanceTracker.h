#pragma once
#include <boost/thread.hpp>

template <typename T>
class instance_tracker
{
public:
	instance_tracker()
	{
		boost::lock_guard<boost::mutex> lg(m);
		++n;
		char buf[128] = {};
		sprintf(buf, "Waveforms>: %d\n", n);
		OutputDebugStringA(buf);
	}

	~instance_tracker()
	{
		boost::lock_guard<boost::mutex> lg(m);
		--n;
		char buf[128] = {};
		sprintf(buf, "Waveforms<: %d\n", n);
		OutputDebugStringA(buf);
	}

private:
	static boost::mutex m;
	static int n;
};

#define DECLARE_INSTANCE_TRACKER(Type) int instance_tracker<Type>::n = 0; boost::mutex instance_tracker<Type>::m;