//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include <boost/filesystem/path.hpp>
#include <vector>

#if defined(BOOST_ALL_NO_LIB)
#  if defined(_DEBUG)
#    pragma comment(lib, "libboost_filesystem-mt-sgd.lib")
#    pragma comment(lib, "libboost_system-mt-sgd.lib")
#  else
#    pragma comment(lib, "libboost_filesystem-mt-s.lib")
#    pragma comment(lib, "libboost_system-mt-s.lib")
#  endif
#endif

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
		std::vector<wchar_t> path(9001);
		HMODULE self;
		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&f, &self);
		GetModuleFileName(self, &path[0], path.size()-1);
		boost::filesystem::path lexer_path = path;
		lexer_path.remove_filename();
		lexer_path /= "SciLexer.dll";

		scintilla.reset(LoadLibrary(lexer_path.c_str()), &FreeLibrary);
	}
}

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT_HOOK(wave::config::frontend_direct3d9, wave::direct3d9::frontend_impl, init_scintilla)