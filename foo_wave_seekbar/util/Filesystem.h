//          Copyright Lars Viklund 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <string>

namespace util {
static bool
starts_with(char const* string, char const* prefix)
{
    while (*string && *prefix && *string == *prefix) {
        ++string;
        ++prefix;
    }
    return *prefix == '\0';
}

static std::wstring
file_location_to_wide_path(char const* fb2k_file)
{
    pfc::string8 native;
    // foobar2000_io::extract_native_path() can strip out file://, but will
    // currently (2011-08-14) break if fed a native path, thus this test.
    if (starts_with(fb2k_file, "file://")) {
        foobar2000_io::extract_native_path(fb2k_file, native);
    } else {
        native = fb2k_file;
    }

    native.replace_byte('/', '\\');

    auto first = native.get_ptr();
    auto w = pfc::stringcvt::string_wide_from_utf8_t<>(first, native.get_length());
    if (w.length() >= 2 && w[0] == L'\\' && w[1] == L'\\') {
        return w.get_ptr();
    }
    return std::wstring(L"\\\\?\\") + w.get_ptr();
}

std::wstring
extract_directory_name(std::wstring path)
{
    auto off = path.find_last_of(L'\\');
    return path.substr(0, off + 1);
}

template<typename F>
void
enumerate_file_glob(std::wstring glob, F f)
{
    WIN32_FIND_DATAW find_data = {};
    auto valid_handle = [](HANDLE h) { return h != INVALID_HANDLE_VALUE; };
    auto search_handle = INVALID_HANDLE_VALUE;
    try {
        search_handle = FindFirstFileW(glob.c_str(), &find_data);
        if (valid_handle(search_handle)) {
            do {
                f(find_data);
            } while (FindNextFileW(search_handle, &find_data));
        }
    } catch (...) {
        if (search_handle != INVALID_HANDLE_VALUE)
            FindClose(search_handle);
        throw;
    }
    if (search_handle != INVALID_HANDLE_VALUE)
        FindClose(search_handle);
}
}
