#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include <boost/extension/extension.hpp>
#include <boost/extension/factory.hpp>
#include <boost/extension/type_map.hpp>
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


BOOST_EXTENSION_TYPE_MAP_FUNCTION {
	using namespace boost::extensions;
	std::map
	<
		wave::config::frontend,
		factory<wave::visual_frontend,
		HWND,
		wave::size,
		wave::visual_frontend_callback&,
		wave::visual_frontend_config&
		>
	>& factories(types.get());

  wchar_t path[9001] = {};
  HMODULE self;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&f, &self);
  GetModuleFileName(self, path, 9000);
  boost::filesystem::path lexer_path = path;
  lexer_path.remove_filename();
  lexer_path /= "SciLexer.dll";

  scintilla.reset(LoadLibrary(lexer_path.c_str()), &FreeLibrary);

	factories[wave::config::frontend_direct3d9].set<wave::direct3d9::frontend_impl>();
}