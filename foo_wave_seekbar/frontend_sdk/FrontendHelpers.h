//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

template<typename T>
inline std::vector<T>
expand_flags(T map)
{
    std::vector<T> ret;
    size_t N = sizeof(T) * 8;
    for (unsigned shift = 0; shift < N; ++shift) {
        T flag = 1 << shift;
        if (map & flag)
            ret.push_back(flag);
    }
    return ret;
}

template<typename Cont>
void
get_resource_contents(Cont& out, WORD id)
{
    out.clear();
    auto module = (HMODULE)&__ImageBase;

    auto res_info = FindResource(module, MAKEINTRESOURCE(id), RT_RCDATA);
    if (!res_info)
        return;

    auto res = LoadResource(module, res_info);
    if (!res)
        return;

    auto size = SizeofResource(module, res_info);
    auto p = (char*)LockResource(res);
    if (!p)
        return;

    std::copy(p, p + size, std::back_inserter(out));
}
