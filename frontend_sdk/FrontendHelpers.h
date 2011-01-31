#pragma once

template <typename T>
inline std::vector<T> expand_flags(T map)
{
	std::vector<T> ret;
	size_t N = sizeof(T) * 8;
	for (unsigned shift = 0; shift < N; ++shift)
	{
		T flag = 1 << shift;
		if (map & flag)
			ret.push_back(flag);
	}
	return ret;
}

template <typename Cont>
void get_resource_contents(Cont& out, WORD id)
{
	auto module = (HMODULE)&__ImageBase;
	auto res_info = FindResource(module, MAKEINTRESOURCE(id), RT_RCDATA);
	auto res = LoadResource(module, res_info);
	auto size = SizeofResource(module, res_info);
	auto p = (char*)LockResource(res);
	std::copy(p, p + size, std::back_inserter(out));
}
