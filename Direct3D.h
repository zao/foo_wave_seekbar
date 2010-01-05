#pragma once
#include "VisualFrontend.h"

namespace wave
{
	bool has_direct3d9();

	struct direct3d9_frontend : visual_frontend
	{
		direct3d9_frontend(HWND wnd, CSize client_size, visual_frontend_callback& callback);
		virtual void clear();
		virtual void draw();
		virtual void present();
		virtual void on_state_changed(state s);

	private:
		void update_effect_colors();
		void update_effect_cursor();
		void update_replaygain();
		void update_data();
		void update_size();
		void update_orientation();
		void update_shade_played();

		CComPtr<IDirect3D9> d3d;
		CComPtr<IDirect3DDevice9> dev;

		CComPtr<IDirect3DTexture9> tex;
		CComPtr<ID3DXEffect> fx;
		CComPtr<IDirect3DVertexBuffer9> vb;
		CComPtr<IDirect3DVertexDeclaration9> decl;

		D3DXHANDLE background_color_var, foreground_color_var, highlight_color_var, selection_color_var;
		D3DXHANDLE cursor_position_var, cursor_visible_var;
		D3DXHANDLE seek_position_var, seeking_var;
		D3DXHANDLE viewport_size_var;
		D3DXHANDLE replaygain_var;
		D3DXHANDLE orientation_var;
		D3DXHANDLE shade_played_var;

		D3DPRESENT_PARAMETERS pp;

		visual_frontend_callback& callback;

	private:
		void create_vertex_resources();
		void release_vertex_resources();
		void create_default_resources();
		void release_default_resources();

		seekbar_state state_copy;

		bool device_lost;
		UINT mip_count;
		bool floating_point_texture;
	};

#if 0
	struct direct3d10 : visual_frontend
	{

		direct3d10(HWND wnd, CSize client_size);
		virtual void clear();
		virtual void draw();
		virtual void present();
		virtual void resize(CSize size);
		virtual void update_effect_colors(window_state& state);
		virtual void update_effect_cursor(window_state& state);
		virtual void update_texture_data(const pfc::list_base_const_t<float>& mini, const pfc::list_base_const_t<float>& maxi, const pfc::list_base_const_t<float>& rms);
		virtual void update_replaygain(window_state& state);

		CComPtr<ID3D10Device> dev;
		CComPtr<IDXGISwapChain> swap_chain;
		CComPtr<ID3D10RenderTargetView> render_target_view;

		CComPtr<ID3D10InputLayout> input_layout;
		CComPtr<ID3D10Texture1D> tex;
		CComPtr<ID3D10Texture1D> stage_tex;
		CComPtr<ID3D10ShaderResourceView> tex_srv;

		CComPtr<ID3D10Effect> fx;
		CComPtr<ID3D10Buffer> vb;

		ID3D10EffectVectorVariable* background_color_var, * highlight_color_var, * selection_color_var, * text_color_var;
		ID3D10EffectScalarVariable* cursor_position_var, * cursor_visible_var;

		UINT mip_count;
	};

	typedef visual_frontend_factory_impl<direct3d10_frontend> direct3d10_frontend_factory;
#endif
}