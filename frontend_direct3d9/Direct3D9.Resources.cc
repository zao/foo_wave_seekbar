//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchDirect3D9.h"
#include "Direct3D9.h"
#include "../frontend_sdk/FrontendHelpers.h"
#include "resource.h"
#include "Direct3D9.Effects.h"

namespace wave
{
	namespace direct3d9
	{
		CComPtr<IDirect3DTexture9> frontend_impl::create_waveform_texture()
		{
			CComPtr<IDirect3DTexture9> tex;
			dev->CreateTexture(2048, 1, mip_count, 0, texture_format, D3DPOOL_MANAGED, &tex, 0);
			return tex;
		}

		void frontend_impl::create_vertex_resources()
		{
			HRESULT hr;

			D3DVERTEXELEMENT9 decls[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
				{ 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};
			hr = dev->CreateVertexDeclaration(decls, &decl);
			if (!SUCCEEDED(hr))
				throw std::exception("Direct3D9: could not create vertex declaration.");
		}

		void frontend_impl::release_vertex_resources()
		{
			decl = 0;
			vb = 0;
		}

		// {55F2D182-2CFF-4C59-81AD-0EF2784E9D0F}
		const GUID guid_fx_string = 
		{ 0x55f2d182, 0x2cff, 0x4c59, { 0x81, 0xad, 0xe, 0xf2, 0x78, 0x4e, 0x9d, 0xf } };
	
		void frontend_impl::create_default_resources()
		{
			HRESULT hr = S_OK;

			shared_ptr<effect_compiler> compiler;
			get_effect_compiler(compiler);

			auto build_effect = [compiler](std::string body) -> shared_ptr<effect_handle>
			{
				shared_ptr<effect_handle> fx;
				std::deque<effect_compiler::diagnostic_entry> errors;
				bool success = compiler->compile_fragment(fx, errors, body);

				if (!success)
				{
					// TODO: propagate errors to parent DLL
					//console::formatter() << "Seekbar: Direct3D: effect compile failed.";
					//std::string text = simple_diagnostic_format(errors);
					//console::formatter() << "Seekbar: Direct3D: " << text.c_str();
				}
				return fx;
			};

			// Compile fallback effect
			{
				std::vector<char> fx_body;
				get_resource_contents(fx_body, IDR_DEFAULT_FX_BODY);
				std::string stored_body(fx_body.begin(), fx_body.end());
				auto fx = build_effect(stored_body);
				if (fx)
					effect_stack.push(fx);
			}

			// Compile current stored effect
			{
				std::string stored_body;
				if (conf.get_configuration_string(guid_fx_string, stored_body))
				{
					shared_ptr<effect_handle> fx;
					fx = build_effect(stored_body);
					if (fx)
						effect_stack.push(fx);
				}
			}

			if (effect_stack.empty())
				throw std::exception("Direct3D9: could not create effects.");
		}

		void frontend_impl::release_default_resources()
		{
			effect_stack = std::stack<shared_ptr<effect_handle>>();
			effect_override.reset();
		}
	}
}
