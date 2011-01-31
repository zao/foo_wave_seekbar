#pragma once

#include "Direct3D9.h"

namespace wave
{
	namespace direct3d9
	{
		struct effect_compiler_impl : effect_compiler
		{
			explicit effect_compiler_impl(CComPtr<IDirect3DDevice9> dev);
			virtual bool compile_fragment(shared_ptr<effect_handle>& effect, std::deque<diagnostic_entry>& output, std::string const& source);

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

		std::string simple_diagnostic_format(std::deque<effect_compiler::diagnostic_entry> const& entries);
	}
}