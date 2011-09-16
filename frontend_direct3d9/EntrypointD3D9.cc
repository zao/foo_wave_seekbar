//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include <boost/filesystem/path.hpp>

static void f() {}

template <class T>
struct deref;

template <typename U>
struct deref<U*>
{
  typedef U type;
};

shared_ptr<deref<HMODULE>::type> scintilla;

void init_scintilla()
{
	if (!scintilla)
	{
		wchar_t path[9001] = {};
		HMODULE self;
		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&f, &self);
		GetModuleFileName(self, path, 9000);
		boost::filesystem::path lexer_path = path;
		lexer_path.remove_filename();
		lexer_path /= "SciLexer.dll";

		scintilla.reset(LoadLibrary(lexer_path.c_str()), &FreeLibrary);
	}
}

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT_HOOK(wave::config::frontend_direct3d9, wave::direct3d9::frontend_impl, init_scintilla)