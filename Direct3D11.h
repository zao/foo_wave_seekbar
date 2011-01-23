#pragma once
#include "VisualFrontend.h"

#include <dxgi.h>
#include <D3Dcommon.h>
#include <d3d11.h>
#include <D3DX10math.h>

namespace wave
{
	bool has_direct3d11();
	namespace direct3d11
	{
		extern const GUID guid_fx11_string;

		struct frontend_impl : visual_frontend
		{
			frontend_impl(HWND wnd, CSize client_size, visual_frontend_callback& callback, visual_frontend_config& conf);
			virtual void clear();
			virtual void draw();
			virtual void present();
			virtual void on_state_changed(state s);

		private:
			visual_frontend_callback& callback;
			visual_frontend_config& conf;

		private:
			CComPtr<IDXGISwapChain> swap_chain;
			CComPtr<ID3D11Device> device;
			CComPtr<ID3D11DeviceContext> device_context;
			CComPtr<ID3D11RenderTargetView> render_target_view;
			CComPtr<ID3D11Texture2D> depth_stencil_buffer;
			CComPtr<ID3D11DepthStencilState> depth_stencil_state;
			CComPtr<ID3D11DepthStencilView> depth_stencil_view;
			CComPtr<ID3D11RasterizerState> raster_state;
		};
	};
}