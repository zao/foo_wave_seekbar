#include "PchSeekbar.h"
#include "Direct3D.h"
#include "SeekbarState.h"
#include "SehHelper.h"

static void reduce_by_two(pfc::list_base_t<float>& data, UINT n)
{
	for (UINT i = 0; i < n; i += 2)
	{
		float avg = (data[i] + data[i + 1]) / 2.0f;
		data.replace_item(i >> 1, avg);
	}
}

static pfc::string get_program_directory()
{
	char* filename;
	_get_pgmptr(&filename);
	pfc::string exe_name = (const char*)filename;
	return pfc::string("file://") + exe_name.subString(0, exe_name.lastIndexOf('\\'));
}

namespace wave
{
	void direct3d9_frontend::on_state_changed(state s)
	{
		if (s & state_size)
			update_size();
		if (s & state_color)
			update_effect_colors();
		if (s & state_position)
			update_effect_cursor();
		if (s & state_replaygain)
			update_replaygain();
		if (s & state_data)
			update_data();
		if (s & state_orientation)
			update_orientation();
		if (s & state_shade_played)
			update_shade_played();
	}

	void direct3d9_frontend::update_effect_colors()
	{
#define UPDATE_COLOR(Name) if (Name##_color_var) { color c = callback.get_color(config::color_##Name); D3DXCOLOR d(c.r, c.g, c.b, c.a); fx->SetVector(Name##_color_var, &D3DXVECTOR4(d)); }
		UPDATE_COLOR(background)
		UPDATE_COLOR(foreground)
		UPDATE_COLOR(highlight)
		UPDATE_COLOR(selection)
#undef UPDATE_COLOR
	}

	void direct3d9_frontend::update_effect_cursor()
	{
		if (cursor_position_var) fx->SetFloat(cursor_position_var, (float)(callback.get_playback_position() / callback.get_track_length()));
		if (cursor_visible_var)  fx->SetBool(cursor_visible_var, callback.is_cursor_visible());
		if (seek_position_var)   fx->SetFloat(seek_position_var, (float)(callback.get_seek_position() / callback.get_track_length()));
		if (seeking_var)         fx->SetBool(seeking_var, callback.is_seeking());
		if (viewport_size_var)   fx->SetVector(viewport_size_var, &D3DXVECTOR4((float)pp.BackBufferWidth, (float)pp.BackBufferHeight, 0, 0));
	}

	void direct3d9_frontend::update_data()
	{
		HRESULT hr = S_OK;
		service_ptr_t<waveform> w;
		if (callback.get_waveform(w)) {
			pfc::list_hybrid_t<float, 2048> avg_min, avg_max, avg_rms;
			w->get_field("minimum", avg_min);
			w->get_field("maximum", avg_max);
			w->get_field("rms", avg_rms);

			for (UINT mip = 0; mip < mip_count; ++mip)
			{
				UINT width = 2048 >> mip;
				D3DLOCKED_RECT lock = {};
				hr = tex->LockRect(mip, &lock, 0, 0);
				if (floating_point_texture)
				{
					D3DXFLOAT16* dst = (D3DXFLOAT16*)lock.pBits;
					for (size_t i = 0; i < width; ++i)
					{
						dst[(i << 2) + 0] = avg_min[i];
						dst[(i << 2) + 1] = avg_max[i];
						dst[(i << 2) + 2] = avg_rms[i];
						dst[(i << 2) + 3] = 0.0f;
					}
				}
				else
				{
					uint32_t* dst = (uint32_t*)lock.pBits;
					for (size_t i = 0; i < width; ++i)
					{
						uint32_t i_sgn = 3;
						uint32_t i_min = (uint32_t)(512.0 * (avg_min[i] + 1.0));
						uint32_t i_max = (uint32_t)(512.0 * (avg_max[i] + 1.0));
						uint32_t i_rms = (uint32_t)(512.0 * (avg_rms[i] + 1.0));
						uint32_t val = ((i_sgn & 0x003) << 30)
						             + ((i_min & 0x3FF) << 20)
						             + ((i_max & 0x3FF) << 10)
						             + ((i_rms & 0x3FF) <<  0);
						dst[i] = val;
					}
				}
				hr = tex->UnlockRect(mip);
				reduce_by_two(avg_min, width);
				reduce_by_two(avg_max, width);
				reduce_by_two(avg_rms, width);
			}
		}
	}

	void direct3d9_frontend::update_replaygain()
	{
		if (replaygain_var)
			fx->SetVector(replaygain_var, &D3DXVECTOR4(
				callback.get_replaygain(visual_frontend_callback::replaygain_album_gain),
				callback.get_replaygain(visual_frontend_callback::replaygain_track_gain),
				callback.get_replaygain(visual_frontend_callback::replaygain_album_peak),
				callback.get_replaygain(visual_frontend_callback::replaygain_track_peak)
				));
	}

	void direct3d9_frontend::update_orientation()
	{
		if (orientation_var)
			fx->SetBool(orientation_var, config::orientation_horizontal == callback.get_orientation());
	}

	void direct3d9_frontend::update_shade_played()
	{
		if (shade_played_var)
			fx->SetBool(shade_played_var, callback.get_shade_played());
	}

	void direct3d9_frontend::create_vertex_resources()
	{
		float buf[] =
		{
			-1.0f, -1.0f,
			-1.0f, 1.0f,
			1.0f, -1.0f,
			1.0f, 1.0f,
		};

		HRESULT hr;
		hr = dev->CreateVertexBuffer(4 * 8, 0, 0, D3DPOOL_MANAGED, &vb, 0);
		if (!SUCCEEDED(hr))
			throw std::exception("Direct3D9: could not create vertex buffer.");

		float* dst = 0;
		hr = vb->Lock(0, 0, (void**)&dst, 0);
		std::copy(buf, buf + 8, dst);
		vb->Unlock();

		D3DVERTEXELEMENT9 decls[] =
		{
			{ 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			D3DDECL_END()
		};
		dev->CreateVertexDeclaration(decls, &decl);
		if (!SUCCEEDED(hr))
			throw std::exception("Direct3D9: could not create vertex declaration.");
	}

	void direct3d9_frontend::release_vertex_resources()
	{
		decl = 0;
		vb = 0;
	}

	void direct3d9_frontend::create_default_resources()
	{
		HRESULT hr = S_OK;
		abort_callback_dummy cb;

		pfc::string profile_directory = core_api::get_profile_path();
		pfc::string program_directory = get_program_directory();

		std::vector<pfc::string> fx_files = list_of
			(profile_directory + "\\effects\\seekbar.fx")
			(program_directory + "\\effects\\seekbar.fx");

		for each (pfc::string fx_file in fx_files)
		{
			if (fx)
				break;
			if (!filesystem::g_exists(fx_file.get_ptr(), cb))
				continue;
			CComPtr<ID3DXBuffer> errors;
			hr = D3DXCreateEffectFromFileA(dev, fx_file.get_ptr() + 7, 0, 0, 0, 0, &fx, &errors);
			if (FAILED(hr))
			{
				console::formatter() << "Seekbar: Direct3D: " << DXGetErrorStringA(hr) << "(" << hr << ") " << DXGetErrorDescriptionA(hr);
				if (errors)
					console::formatter() << "Seekbar: Direct3D: " << pfc::string8((char*)errors->GetBufferPointer(), errors->GetBufferSize());
			}
		}

		if (!fx)
			throw std::exception("Direct3D9: could not create effects.");

		fx->SetTexture(fx->GetParameterBySemantic(0, "WAVEFORMDATA"), tex);

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

	void direct3d9_frontend::release_default_resources()
	{
		background_color_var = foreground_color_var = highlight_color_var = selection_color_var = 0;
		cursor_position_var = 0;
		cursor_visible_var = 0;
		seek_position_var = 0;
		seeking_var = 0;
		replaygain_var = 0;
		orientation_var = 0;
		shade_played_var = 0;
		fx.Release();
	}

	bool has_direct3d9() {
		HMODULE d3d9_lib = LoadLibrary(L"d3d9");
		FreeLibrary(d3d9_lib);
#ifdef _DEBUG
		HMODULE d3d9x_lib = LoadLibrary(L"d3dx9d_42");
#else
		HMODULE d3d9x_lib = LoadLibrary(L"d3dx9_42");
#endif
		FreeLibrary(d3d9x_lib);
		return !!d3d9_lib && !!d3d9x_lib;
	}

	struct create_d3d9_func
	{
		IDirect3D9* operator() () const
		{
			IDirect3D9* p = Direct3DCreate9(D3D_SDK_VERSION);
			return p;
		}
	};

	struct test_d3dx9_func
	{
		bool operator() () const
		{
			D3DXMATRIX m;
			D3DXMatrixTranslation(&m, 0, 0, 0);
			return true;
		}
	};

	direct3d9_frontend::direct3d9_frontend(HWND wnd, CSize client_size, visual_frontend_callback& callback)
		: mip_count(4), callback(callback), floating_point_texture(true)
	{
		HRESULT hr = S_OK;

		d3d.Attach(try_module_call(create_d3d9_func()));
		bool has_d3dx = try_module_call(test_d3dx9_func());

		if (!d3d || !has_d3dx)
		{
			throw std::runtime_error("DirectX redistributable not found. Run the DirectX August 2009 web setup or later.");
		}

		ZeroMemory(&pp, sizeof(pp));
		pp.BackBufferWidth = client_size.cx;
		pp.BackBufferHeight = client_size.cy;
		pp.BackBufferFormat = D3DFMT_A8R8G8B8;
		pp.BackBufferCount = 1;
		pp.MultiSampleType = D3DMULTISAMPLE_NONE;
		pp.MultiSampleQuality = 0;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.hDeviceWindow = wnd;
		pp.Windowed = TRUE;
		pp.EnableAutoDepthStencil = FALSE;
		pp.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
		pp.Flags = 0;
		pp.FullScreen_RefreshRateInHz = 0;
		pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

		DWORD msaa_quality = 0;
		for (INT x = D3DMULTISAMPLE_16_SAMPLES; x >= 0; --x)
		{
			D3DMULTISAMPLE_TYPE msaa_type = (D3DMULTISAMPLE_TYPE)x;
			if (SUCCEEDED(d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_A8R8G8B8, TRUE, msaa_type, &msaa_quality)))
			{
				pp.MultiSampleType = msaa_type;
				pp.MultiSampleQuality = msaa_quality - 1;
				break;
			}
		}

		hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, wnd, D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
		if (!SUCCEEDED(hr))
		{
			pp.MultiSampleType = D3DMULTISAMPLE_NONE;
			pp.MultiSampleQuality = 0;
			hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, wnd, D3DCREATE_FPU_PRESERVE | D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
			if (!SUCCEEDED(hr))
				throw std::exception("Direct3D9: could not create device.");
		}

		auto create_data_texture = [this](D3DFORMAT fmt) { return dev->CreateTexture(2048, 1, mip_count, 0, D3DFMT_A16B16G16R16F, D3DPOOL_MANAGED, &tex, 0); };
		hr = create_data_texture(D3DFMT_A16B16G16R16F);
		if (!SUCCEEDED(hr))
		{
			floating_point_texture = false;
			hr = create_data_texture(D3DFMT_A2R10G10B10);
			if (!SUCCEEDED(hr))
			{
				hr = create_data_texture(D3DFMT_A8R8G8B8);
				if (!SUCCEEDED(hr))
					throw std::exception("Direct3D9: could not create texture.");
			}
		}

		create_vertex_resources();
		create_default_resources();
		on_state_changed((state)~0);
	}

	void direct3d9_frontend::clear()
	{
		color c = callback.get_color(config::color_background);
		D3DXCOLOR bg(c.r, c.g, c.b, c.a);
		dev->Clear(0, 0, D3DCLEAR_TARGET, bg, 1.0f, 0);
	}

	void direct3d9_frontend::draw()
	{
		HRESULT hr = S_OK;
		hr = dev->BeginScene();
		hr = dev->SetStreamSource(0, vb, 0, 8);
		hr = dev->SetTexture(0, tex);
		hr = dev->SetVertexDeclaration(decl);
		UINT passes = 0;
		fx->Begin(&passes, 0);
		for (UINT pass = 0; pass < passes; ++pass)
		{
			fx->BeginPass(pass);
			fx->CommitChanges();
			hr = dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
			fx->EndPass();
		}
		fx->End();
		hr = dev->EndScene();
	}

	void direct3d9_frontend::present()
	{
		if (D3DERR_DEVICELOST == dev->Present(0, 0, 0, 0))
		{
			device_lost = true;
			update_size();
		}
	}

	void direct3d9_frontend::update_size()
	{
		release_default_resources();
		CSize size = callback.get_size();
		pp.BackBufferWidth = size.cx;
		pp.BackBufferHeight = size.cy;
		HRESULT hr = S_OK;
		do
			hr = dev->Reset(&pp);
		while (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET);
		create_default_resources();
		update_effect_colors();
		update_effect_cursor();
		update_replaygain();
		update_orientation();
		update_shade_played();
		device_lost = false;
	}
}