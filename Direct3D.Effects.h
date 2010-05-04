#pragma once

#include "Direct3D.h"

namespace wave
{
	namespace direct3d9
	{
		struct effect_compiler_impl : effect_compiler
		{
			explicit effect_compiler_impl(CComPtr<IDirect3DDevice9> dev);
			virtual bool compile_fragment(service_ptr_t<effect_handle>& effect, pfc::list_t<diagnostic_entry>& output, pfc::string const& source);

		private:
			CComPtr<IDirect3DDevice9> dev;
		};

		struct effect_impl : effect_handle
		{
			explicit effect_impl(CComPtr<ID3DXEffect> fx);

			CComPtr<ID3DXEffect> get_effect() const;

		private:
			CComPtr<ID3DXEffect> fx;
		};

		pfc::string simple_diagnostic_format(pfc::list_t<effect_compiler::diagnostic_entry> const& entries);
	}
}