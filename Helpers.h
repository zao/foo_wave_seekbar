#pragma once

template <typename F>
auto try_module_call(F f) -> decltype(f())
{
	typedef decltype(f()) return_type;
	return_type ret = return_type();
	__try
	{
		ret = f();
	}
	__except(GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{}
	return ret;
}

template <typename F>
void in_main_thread(F f)
{
	struct in_main : main_thread_callback
	{
		void callback_run() override
		{
			f();
		}

		in_main(F f) : f(f) {}
		F f;
	};

	static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<in_main>(f));
}

inline float downmix(pfc::list_t<float> const& frame)
{
	const audio_sample sqrt_half = audio_sample(0.70710678118654752440084436210485);
	pfc::list_t<float> data = frame;
	switch (data.get_size())
	{
	case 8:
		data[0] += frame[6] * sqrt_half;
		data[1] += frame[7] * sqrt_half;
	case 6:
		data[0] += frame[2] * sqrt_half + frame[4] * sqrt_half + frame[3];
		data[1] += frame[2] * sqrt_half + frame[5] * sqrt_half + frame[3];
	case 2:
		data[0] += frame[1];
		data[0] /= 2.0;
		break;
	case 4:
		data[0] += frame[1] + frame[2] + frame[3];
		data[0] /= 4.0;
	}
	return data[0];
}

inline unsigned count_bits_set(unsigned v)
{
	v = v - ((v >> 1) & 0x55555555);                       // reuse input as temporary
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);        // temp
	return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24; // count
}