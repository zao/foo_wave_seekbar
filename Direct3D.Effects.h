#pragma once

#include "Direct3D.h"

namespace wave
{
	namespace direct3d9
	{
		struct effect_compiler_impl : effect_compiler
		{
			effect_compiler_impl(weak_ptr<frontend_impl> fe, CComPtr<IDirect3DDevice9> dev);
			virtual bool compile_fragment(service_ptr_t<effect_handle>& effect, pfc::list_t<diagnostic_entry>& output, pfc::string const& source);

		private:
			weak_ptr<frontend_impl> fe;
			CComPtr<IDirect3DDevice9> dev;
			std::vector<char> fx_header, fx_footer;
			size_t offset_lines;
		};
	}
}