#pragma once
#include <windows.h>

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

template <typename T>
inline std::vector<T> expand_flags(T map)
{
	std::vector<T> ret;
	size_t N = sizeof(T) * 8;
	for (unsigned shift = 0; shift < N; ++shift)
	{
		T flag = 1 << shift;
		if (map & flag)
			ret += flag;
	}
	return ret;
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

template <typename Cont>
void get_resource_contents(Cont& out, WORD id)
{
	auto module = core_api::get_my_instance();
	auto res_info = FindResource(module, MAKEINTRESOURCE(id), RT_RCDATA);
	auto res = LoadResource(module, res_info);
	auto size = SizeofResource(module, res_info);
	auto p = (char*)LockResource(res);
	std::copy(p, p + size, std::back_inserter(out));
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

template <typename T>
T clamp(T v, T a, T b)
{
	return std::max(a, std::min(b, v));
}

inline bool is_outside(CPoint point, CRect r, int N, bool horizontal)
{
	if (!horizontal)
	{
		std::swap(point.x, point.y);
		std::swap(r.right, r.bottom);
		std::swap(r.left, r.top);
	}
	return point.y < -2 * N || point.y > r.bottom - r.top + 2 * N ||
		    point.x < -N     || point.x > r.right - r.left + N;
}