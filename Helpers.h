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