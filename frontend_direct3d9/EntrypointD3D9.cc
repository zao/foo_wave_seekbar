//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include <cstdint>
#include <vector>

static void f() {}

template <class T>
struct deref;

template <typename U>
struct deref<U*>
{
  typedef U type;
};

std::shared_ptr<deref<HMODULE>::type> scintilla;

static void replace_filename(std::vector<wchar_t>& v, std::wstring const& new_name)
{
	size_t last_component = 0u;
	for (size_t i = 0; v[i] != '\0'; ++i) {
		if (v[i] == '/' || v[i] == '\\') {
			last_component = i+1;
		}
	}
	if (v.size() > last_component + new_name.size() + 1) {
		memcpy(&v[last_component], new_name.c_str(), (1+new_name.size()) * sizeof(wchar_t));
	}
}

void init_scintilla()
{
	if (!scintilla)
	{
		std::vector<wchar_t> path(9001);
		HMODULE self;
		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&f, &self);
		GetModuleFileName(self, &path[0], path.size()-1);
		replace_filename(path, L"SciLexer.dll");

		scintilla.reset(LoadLibraryW(path.data()), &FreeLibrary);
	}
}

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT_HOOK(wave::config::frontend_direct3d9, wave::direct3d9::frontend_impl, init_scintilla)