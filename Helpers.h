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