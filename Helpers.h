//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <windows.h>
#include "../SDK/foobar2000.h"

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

const DWORD MS_VC_EXCEPTION=0x406D1388;

inline void SetThreadName( DWORD dwThreadID, char const* threadName)
{
	#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
	   DWORD dwType; // Must be 0x1000.
	   LPCSTR szName; // Pointer to name (in user addr space).
	   DWORD dwThreadID; // Thread ID (-1=caller thread).
	   DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
	#pragma pack(pop)

	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
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

template <typename T>
inline T lerp(T a, T b, float n)
{
	return (1.0f - n)*a + n*b;
}

inline bool less_guid(GUID const& a, GUID const& b)
{
	return memcmp(&a.Data1, &b.Data1, sizeof(GUID)) < 0;
}

inline DWORD xbgr_to_argb(COLORREF c, BYTE a = 0xFFU)
{
	return (a << 24) | (GetRValue(c) << 16) | (GetGValue(c) << 8) | (GetBValue(c));
}

template <typename Cont, typename Pred>
typename Cont::size_type nuke_if(Cont& c, Pred p)
{
	auto count = c.size();
	c.erase(std::remove_if(begin(c), end(c), p), end(c));
	return count - c.size();
}

inline void reduce_by_two(pfc::list_base_t<float>& data, UINT n)
{
	for (UINT i = 0; i < n; i += 2)
	{
		float avg = (data[i] + data[i + 1]) / 2.0f;
		data.replace_item(i >> 1, avg);
	}
}

inline pfc::string get_program_directory()
{
	char* filename;
	_get_pgmptr(&filename);
	pfc::string exe_name = (const char*)filename;
	return pfc::string("file://") + exe_name.subString(0, exe_name.lastIndexOf('\\'));
}

inline unsigned count_bits_set(unsigned v)
{
	v = v - ((v >> 1) & 0x55555555);                       // reuse input as temporary
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);        // temp
	return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24; // count
}

template <typename T>
T clamp(T v, T a, T b)
{
	return std::max(a, std::min(b, v));
}
