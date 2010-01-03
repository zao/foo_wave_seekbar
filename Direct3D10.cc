#include "PchSeekbar.h"
#include "Direct3D.h"
#include "SeekbarState.h"

namespace wave
{
#if 0
	void direct3d10::update_effect_colors(window_state& state)
	{
		TRACK_CALL_TEXT("update_effect_colors");
	#define UPDATE_COLOR(Name) if (Name##_var && Name##_var->IsValid()) { Name##_var->SetFloatVector(state.Name); }
		UPDATE_COLOR(background_color)
			UPDATE_COLOR(highlight_color)
			UPDATE_COLOR(selection_color)
			UPDATE_COLOR(text_color)
	#undef UPDATE_COLOR
	}

	void direct3d10::update_effect_cursor(window_state& state)
	{
		TRACK_CALL_TEXT("update_effect_cursor");
		ID3D10EffectScalarVariable* cpv = cursor_position_var;
		if (cpv && cpv->IsValid())
			cpv->SetFloat((float)(state.cursor_position / state.track_length));
		ID3D10EffectScalarVariable* cvv = cursor_visible_var;
		if (cvv && cvv->IsValid())
			cvv->SetBool(state.cursor_is_visible);
	}

	void direct3d10::update_texture_data(const pfc::list_base_const_t<float>& mini, const pfc::list_base_const_t<float>& maxi, const pfc::list_base_const_t<float>& rms)
	{
		TRACK_CALL_TEXT("update_texture_data");
		float* dst = 0;
		pfc::list_hybrid_t<float, 2048> avg_min, avg_max, avg_rms;
		avg_min.add_items(mini);
		avg_max.add_items(maxi);
		avg_rms.add_items(rms);
		for (UINT mip = 0; mip < mip_count; ++mip)
		{
			UINT sub_index = D3D10CalcSubresource(mip, 0, mip_count);
			stage_tex->Map(sub_index, D3D10_MAP_WRITE, 0, (void**)&dst);
			UINT width = 2048U >> mip;
			for (UINT i = 0; i < width; ++i)
			{
				dst[i << 1] = avg_min[i];
				dst[(i << 1) + 1] = avg_max[i];
				dst[(i << 1) + 2] = avg_rms[i];
			}
			stage_tex->Unmap(sub_index);
			
			reduce_by_two(avg_min, width);
			reduce_by_two(avg_max, width);
			reduce_by_two(avg_rms, width);
		}
		dev->CopyResource(stage_tex, tex);
	}

	void direct3d10::update_replaygain(window_state& state)
	{
		// TODO: stub
	}

	void find_perf_hud(D3D10_DRIVER_TYPE& driver_type, CComPtr<IDXGIAdapter>& selectedAdapter)
	{
		driver_type = D3D10_DRIVER_TYPE_HARDWARE;
		CComPtr<IDXGIFactory> dxgi;
		CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&dxgi);
		UINT n_adapter = 0;
		CComPtr<IDXGIAdapter> adapter;
		while (dxgi->EnumAdapters(n_adapter, &adapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC desc;
			if (SUCCEEDED(adapter->GetDesc(&desc)))
			{
				bool is_perf_hud = wcscmp(desc.Description, L"NVIDIA PerfHUD") == 0;
				if (n_adapter == 0 || is_perf_hud)
					selectedAdapter = adapter;
				if (is_perf_hud)
					driver_type = D3D10_DRIVER_TYPE_REFERENCE;
			}
			++n_adapter;
			adapter.Release();
		}
	}

	direct3d10::direct3d10(HWND wnd, CSize client_size)
		: background_color_var(0), highlight_color_var(0), selection_color_var(0), text_color_var(0)
		, cursor_position_var(0), cursor_visible_var(0), mip_count(4)
	{
		TRACK_CALL_TEXT("direct3d10");
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = client_size.cx;
		sd.BufferDesc.Height = client_size.cy;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = wnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Windowed = TRUE;

#undef FEATURE_LEVELS
#ifdef FEATURE_LEVELS
		std::vector<D3D10_FEATURE_LEVEL1> levels = list_of
			(D3D10_FEATURE_LEVEL_10_1)
			(D3D10_FEATURE_LEVEL_10_0)
			(D3D10_FEATURE_LEVEL_9_3)
			(D3D10_FEATURE_LEVEL_9_2)
			(D3D10_FEATURE_LEVEL_9_1);

		const wchar_t* level_name[] =
		{
#define OMG_STR(x) L#x
			OMG_STR(D3D10_FEATURE_LEVEL_10_1),
			OMG_STR(D3D10_FEATURE_LEVEL_10_0),
			OMG_STR(D3D10_FEATURE_LEVEL_9_3),
			OMG_STR(D3D10_FEATURE_LEVEL_9_2),
			OMG_STR(D3D10_FEATURE_LEVEL_9_1),
		};
#endif

#ifdef DEBUG
		int flags = D3D10_CREATE_DEVICE_DEBUG;
#else
		int flags = 0;
#endif

		D3D10_DRIVER_TYPE driver_type;
		CComPtr<IDXGIAdapter> adapter;
		find_perf_hud(driver_type, adapter);

#undef OutputDebugString
#define OutputDebugString(x) do { std::wstring msg = (x); MessageBox(0, msg.c_str(), msg.c_str(), MB_OK); } while(0);
		
		bool success = false;
		{
			TRACK_CALL_TEXT("create_device");
			HRESULT hr = D3D10CreateDeviceAndSwapChain(adapter, driver_type, 0, flags, D3D10_SDK_VERSION, &sd, &swap_chain, &dev);
			if (success = SUCCEEDED(hr))
			{
				OutputDebugString(L"Device creation succeeded.\n");
			}
			else
			{
				OutputDebugString(str(boost::wformat(L"Device creation failed: %s.\n") % DXGetErrorStringW(hr)).c_str());
			}
		}
		if (success)
		{
			abort_callback_dummy cb;
			{
				TRACK_CALL_TEXT("rtv");
				CComPtr<ID3D10Texture2D> backBuffer;
				swap_chain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)&backBuffer);
				dev->CreateRenderTargetView(backBuffer, 0, &render_target_view);
				backBuffer.Release();
				dev->OMSetRenderTargets(1, &render_target_view.p, 0);
				D3D10_VIEWPORT vp = {};
				vp.Width = client_size.cx;
				vp.Height = client_size.cy;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				vp.TopLeftX = 0;
				vp.TopLeftY = 0;
				dev->RSSetViewports(1, &vp);
			}

			{
				TRACK_CALL_TEXT("fx");
				pfc::string profile_directory = core_api::get_profile_path();
				pfc::string program_directory = get_program_directory();
				
				std::vector<pfc::string> fx_files = list_of
					(profile_directory + "\\effects\\seekbar.fx")
					(program_directory + "\\effects\\seekbar.fx");

				std::vector<const char*> profiles;
#ifndef FEATURE_LEVELS
				D3D10_FEATURE_LEVEL1 level = D3D10_FEATURE_LEVEL_10_0;
#endif
				switch (level)
				{
				case D3D10_FEATURE_LEVEL_10_1:
					profiles += "fx_4_1";
				case D3D10_FEATURE_LEVEL_10_0:
					profiles += "fx_4_0";
				case D3D10_FEATURE_LEVEL_9_3:
				case D3D10_FEATURE_LEVEL_9_2:
				case D3D10_FEATURE_LEVEL_9_1:
					profiles += "fx_2_0";
				}

	#ifdef DEBUG
				UINT hlsl_flags = D3D10_SHADER_DEBUG;
	#else
				UINT hlsl_flags = 0;
	#endif
				BOOST_FOREACH(pfc::string fx_file, fx_files)
				{
					if (fx)
						break;
					if (!filesystem::g_exists(fx_file.get_ptr(), cb))
						continue;
					BOOST_FOREACH(const char* profile, profiles)
					{
						CComPtr<ID3D10Blob> errors;
						try
						{
							HRESULT hr;
							hr = D3DX10CreateEffectFromFileA(fx_file.get_ptr() + 7, 0, 0,
								profile, hlsl_flags, 0, dev, 0, 0, &fx, &errors, 0);
							if (success = SUCCEEDED(hr))
								break;
						}
						catch (...)
						{
							if (errors)
							{
								OutputDebugStringA(pfc::string((const char*)errors->GetBufferPointer(), errors->GetBufferSize()).get_ptr());
								OutputDebugStringA("\n");
							}
						}
					}
				}
			}

			if (success)
			{
				{
					// TODO: 3-channel texture
					TRACK_CALL_TEXT("tex");
					D3D10_TEXTURE1D_DESC desc = {};
					desc.ArraySize = 1;
					desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
					desc.CPUAccessFlags = 0;
					desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
					desc.MipLevels = mip_count;
					desc.MiscFlags = 0;
					desc.Usage = D3D10_USAGE_DEFAULT;
					desc.Width = 2048;
					dev->CreateTexture1D(&desc, 0, &tex);

					desc.BindFlags = 0;
					desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
					desc.Usage = D3D10_USAGE_STAGING;
					dev->CreateTexture1D(&desc, 0, &stage_tex);					

					D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
					srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
					srv_desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
					srv_desc.Texture1D.MipLevels = mip_count;
					srv_desc.Texture1D.MostDetailedMip = 0;

					dev->CreateShaderResourceView(tex, &srv_desc, &tex_srv);

					ID3D10EffectShaderResourceVariable* tex_var = fx->GetVariableBySemantic("WAVEFORMDATA")->AsShaderResource();
					if (tex_var->IsValid())
					{
						tex_var->SetResource(tex_srv);
					}

					background_color_var = fx->GetVariableBySemantic("BACKGROUNDCOLOR")->AsVector();
					highlight_color_var = fx->GetVariableBySemantic("HIGHLIGHTCOLOR")->AsVector();
					selection_color_var = fx->GetVariableBySemantic("SELECTIONCOLOR")->AsVector();
					text_color_var = fx->GetVariableBySemantic("TEXTCOLOR")->AsVector();

					cursor_position_var = fx->GetVariableBySemantic("CURSORPOSITION")->AsScalar();
					cursor_visible_var = fx->GetVariableBySemantic("CURSORVISIBLE")->AsScalar();
				}

				{
					TRACK_CALL_TEXT("vertices");
					D3D10_INPUT_ELEMENT_DESC input_desc[] =
					{
						{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0 }
					};
					D3D10_PASS_DESC pass_desc = {};
					fx->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetDesc(&pass_desc);
					dev->CreateInputLayout(input_desc, 1, pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize, &input_layout);

					D3D10_BUFFER_DESC buffer_desc = {};
					buffer_desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
					buffer_desc.ByteWidth = 4 * 8;
					buffer_desc.CPUAccessFlags = 0;
					buffer_desc.MiscFlags = 0;
					buffer_desc.Usage = D3D10_USAGE_IMMUTABLE;

					float buf[] =
					{
						-1.0f, -1.0f,
						-1.0f, 1.0f,
						1.0f, -1.0f,
						1.0f, 1.0f,
					};
					D3D10_SUBRESOURCE_DATA sr_data = {};
					sr_data.pSysMem = buf;
					sr_data.SysMemPitch = 4 * 8;
					dev->CreateBuffer(&buffer_desc, &sr_data, &vb);
				}
			}
		}
	}

	void direct3d10::clear(D3DXCOLOR bg)
	{
		TRACK_CALL_TEXT("direct3d10::clear");
		dev->ClearRenderTargetView(render_target_view, bg);
	}

	void direct3d10::draw()
	{
		TRACK_CALL_TEXT("direct3d10::draw");
		dev->IASetInputLayout(input_layout);
		dev->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		UINT stride = 8, offset = 0;
		dev->IASetVertexBuffers(0, 1, &vb.p, &stride, &offset);

		D3D10_TECHNIQUE_DESC tech_desc = {};
		ID3D10EffectTechnique* tech = fx->GetTechniqueByIndex(0);
		tech->GetDesc(&tech_desc);
		for (UINT pass = 0; pass < tech_desc.Passes; ++pass)
		{
			tech->GetPassByIndex(pass)->Apply(0);
			dev->Draw(4, 0);
		}

	}

	void direct3d10::present()
	{
		TRACK_CALL_TEXT("direct3d10::present");
		swap_chain->Present(0, 0);
	}

	void direct3d10::resize(CSize size)
	{
		if (swap_chain && size.cx > 1 && size.cy > 1)
		{
			TRACK_CALL_TEXT("direct3d10::on_resize");
			dev->OMSetRenderTargets(0, 0, 0);
			render_target_view.Release();
			swap_chain->ResizeBuffers(1, size.cx, size.cy, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
			CComPtr<ID3D10Texture2D> backBuffer;
			swap_chain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)&backBuffer);
			dev->CreateRenderTargetView(backBuffer, 0, &render_target_view);
			backBuffer.Release();
			dev->OMSetRenderTargets(1, &render_target_view.p, 0);
			D3D10_VIEWPORT vp = {};
			vp.Width = size.cx;
			vp.Height = size.cy;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			dev->RSSetViewports(1, &vp);
		}
	}
#endif
}