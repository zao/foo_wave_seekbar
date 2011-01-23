#include "PchSeekbar.h"
#include "Direct3D11.h"

namespace wave
{
	bool has_direct3d11()
	{
		return true;
	}

	namespace direct3d11
	{
		frontend_impl::frontend_impl(HWND wnd, CSize client_size, visual_frontend_callback& callback, visual_frontend_config& conf)
			: callback(callback), conf(conf)
		{
			HRESULT hr = S_OK;

			DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
			D3D_FEATURE_LEVEL feature_level;
			CComPtr<ID3D11Texture2D> back_buffer;
			D3D11_TEXTURE2D_DESC depth_buffer_desc = {};
			D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {};
			D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc = {};
			D3D11_RASTERIZER_DESC raster_desc = {};
			D3D11_VIEWPORT viewport = {};

			// prepare the swap chain descriptor
			{
				auto& scd = swap_chain_desc;
				scd.BufferCount = 1;
				scd.BufferDesc.Width = client_size.cx;
				scd.BufferDesc.Height = client_size.cy;

				scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

				scd.BufferDesc.RefreshRate.Numerator = 0;
				scd.BufferDesc.RefreshRate.Denominator = 1;

				scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

				scd.OutputWindow = wnd;

				scd.SampleDesc.Count = 1;
				scd.SampleDesc.Quality = 0;

				scd.Windowed = true;
				
				scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
				scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

				scd.Flags = 0;
			}

			feature_level = D3D_FEATURE_LEVEL_11_0;

			// create device, swap chain, RTVs, etc.
			hr =D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
				nullptr, 0, D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device,
				&feature_level, &device_context);

			hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
			hr = device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);

			{
				auto& dbd = depth_buffer_desc;
				
				dbd.Width = client_size.cx;
				dbd.Height = client_size.cy;
				dbd.MipLevels = 1;
				dbd.ArraySize = 1;
				dbd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				dbd.SampleDesc.Count = 1;
				dbd.SampleDesc.Quality = 0;
				dbd.Usage = D3D11_USAGE_DEFAULT;
				dbd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				dbd.CPUAccessFlags = 0;
				dbd.MiscFlags = 0;
			}

			hr = device->CreateTexture2D(&depth_buffer_desc, nullptr, &depth_stencil_buffer);

			{
				auto& dsd = depth_stencil_desc;

				dsd.DepthEnable = true;
				dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
				dsd.DepthFunc = D3D11_COMPARISON_LESS;
				
				dsd.StencilEnable = true;
				dsd.StencilReadMask = 0xFF;
				dsd.StencilWriteMask = 0xFF;

				dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
				dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
				dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
				dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

				dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
				dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
				dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
				dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			}

			hr = device->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state);
			device_context->OMSetDepthStencilState(depth_stencil_state, 1);

			{
				auto& dsvd = depth_stencil_view_desc;

				dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				dsvd.Texture2D.MipSlice = 0;
			}

			hr = device->CreateDepthStencilView(depth_stencil_buffer, &depth_stencil_view_desc, &depth_stencil_view);
			device_context->OMSetRenderTargets(1, &render_target_view.p, depth_stencil_view);

			{
				auto& rd = raster_desc;

				rd.AntialiasedLineEnable = false;
				rd.CullMode = D3D11_CULL_BACK;
				rd.DepthBias = 0;
				rd.DepthBiasClamp = 0.0f;
				rd.DepthClipEnable = true;
				rd.FillMode = D3D11_FILL_SOLID;
				rd.FrontCounterClockwise = false;
				rd.MultisampleEnable = false;
				rd.ScissorEnable = false;
				rd.SlopeScaledDepthBias = 0.0f;
			}

			hr = device->CreateRasterizerState(&raster_desc, &raster_state);
			device_context->RSSetState(raster_state);

			{
				auto& vp = viewport;

				vp.Width = (float)client_size.cx;
				vp.Height = (float)client_size.cy;
				vp.MinDepth = 0.0f;
				vp.MaxDepth = 1.0f;
				vp.TopLeftX = 0.0f;
				vp.TopLeftY = 0.0f;
			}

			device_context->RSSetViewports(1, &viewport);
		}

		void frontend_impl::clear()
		{
		}

		void frontend_impl::draw()
		{
		}

		void frontend_impl::present()
		{
		}

		void frontend_impl::on_state_changed(state s)
		{
		}
	}
}
