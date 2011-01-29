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