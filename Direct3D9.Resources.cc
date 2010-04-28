#include "PchSeekbar.h"
#include "Direct3D.h"
#include "Helpers.h"
#include "resource.h"

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
			abort_callback_dummy cb;

			pfc::string profile_directory = core_api::get_profile_path();
			pfc::string program_directory = get_program_directory();

			std::vector<char> fx_header, fx_body, fx_footer;
			get_resource_contents(fx_header, IDR_DEFAULT_FX_HEADER);
			get_resource_contents(fx_footer, IDR_DEFAULT_FX_FOOTER);

			pfc::string stored_body;
			if (conf.get_configuration_string(guid_fx_string, stored_body))
			{
				auto p = stored_body.get_ptr();
				fx_body.assign(p, p + stored_body.get_length());
			}
			else
			{
				get_resource_contents(fx_body, IDR_DEFAULT_FX_BODY);
				stored_body.set_string(&fx_body[0], fx_body.size());
				conf.set_configuration_string(guid_fx_string, stored_body);
			}

			if (size_t diff = nuke_if(fx_body, [](char c) { return (unsigned char)c >= 0x80U; }))
			{
					console::formatter() << "Seekbar: Direct3D: effect contained non-ASCII code units, discarded "
										 << diff << " code units. Remove any UTF-8 BOM and/or characters with diacritics.";
			}

			std::string fx_source;
			{
				using namespace karma;
				generate(std::back_inserter(fx_source), *char_ << "\n" << *char_ << "\n" << *char_ << "\n", fx_header, fx_body, fx_footer);
			}

			{
				CComPtr<ID3DXBuffer> errors;
				hr = D3DXCreateEffect(dev, &fx_source[0], fx_source.size(), nullptr, nullptr, 0, nullptr, &fx, &errors);
				if (FAILED(hr))
				{
					console::formatter() << "Seekbar: Direct3D: " << DXGetErrorStringA(hr) << "(" << hr << ") " << DXGetErrorDescriptionA(hr);
					if (errors)
						console::formatter() << "Seekbar: Direct3D: " << pfc::string8((char*)errors->GetBufferPointer(), errors->GetBufferSize());
				}
			}

			if (!fx)
				throw std::exception("Direct3D9: could not create effects.");

			D3DXHANDLE h = 0;
			for (int i = 0; h = fx->GetParameter(0, i); ++i)
			{
				D3DXPARAMETER_DESC pd = {};
				fx->GetParameterDesc(h, &pd);
				if (pd.Type == D3DXPT_TEXTURE2D)
				{
					for (UINT ann_idx = 0; ann_idx < pd.Annotations; ++ann_idx)
					{
						D3DXHANDLE ann = fx->GetAnnotation(h, ann_idx);
						D3DXPARAMETER_DESC ann_desc = {};
						fx->GetParameterDesc(ann, &ann_desc);
						if (ann_desc.Type == D3DXPT_STRING && ann_desc.Name == std::string("ResourceName"))
						{
							char const* resource_name = 0;
							fx->GetString(ann, &resource_name);

							CComPtr<IDirect3DTexture9> ann_tex;
							hr = D3DXCreateTextureFromFileA(dev, resource_name, &ann_tex);
							if (!SUCCEEDED(hr))
							{
								pfc::string file = profile_directory + "\\effects\\" + resource_name;
								hr = D3DXCreateTextureFromFileA(dev, file.get_ptr() + 7, &ann_tex);
								if (!SUCCEEDED(hr))
								{
									console::formatter() << "Wave seekbar: Direct3D9: Could not load annotation texture " << resource_name;
								}
							}
							if (ann_tex)
							{
								annotation_textures.push_back(ann_tex);
								fx->SetTexture(h, ann_tex);
							}
						}
					}
				}
			}

			background_color_var = fx->GetParameterBySemantic(0, "BACKGROUNDCOLOR");
			foreground_color_var = fx->GetParameterBySemantic(0, "TEXTCOLOR");
			highlight_color_var = fx->GetParameterBySemantic(0, "HIGHLIGHTCOLOR");
			selection_color_var = fx->GetParameterBySemantic(0, "SELECTIONCOLOR");

			cursor_position_var = fx->GetParameterBySemantic(0, "CURSORPOSITION");
			cursor_visible_var = fx->GetParameterBySemantic(0, "CURSORVISIBLE");

			seek_position_var = fx->GetParameterBySemantic(0, "SEEKPOSITION");
			seeking_var = fx->GetParameterBySemantic(0, "SEEKING");

			viewport_size_var = fx->GetParameterBySemantic(0, "VIEWPORTSIZE");
			replaygain_var = fx->GetParameterBySemantic(0, "REPLAYGAIN");

			orientation_var = fx->GetParameterBySemantic(0, "ORIENTATION");
			shade_played_var = fx->GetParameterBySemantic(0, "SHADEPLAYED");
		}

		void frontend_impl::release_default_resources()
		{
			background_color_var = foreground_color_var = highlight_color_var = selection_color_var = 0;
			cursor_position_var = 0;
			cursor_visible_var = 0;
			seek_position_var = 0;
			seeking_var = 0;
			viewport_size_var = 0;
			replaygain_var = 0;
			orientation_var = 0;
			shade_played_var = 0;

			annotation_textures.clear();
			fx.Release();
		}
	}
}