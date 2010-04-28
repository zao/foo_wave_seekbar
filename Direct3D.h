#pragma once
#include "VisualFrontend.h"
#include <map>
#include "resource.h"
#include <boost/fusion/include/adapted.hpp>

namespace wave
{
	bool has_direct3d9();

	namespace direct3d9
	{
		struct config_dialog;
		struct effect_compiler;
		struct effect_handle;

		extern const GUID guid_fx_string;

		struct frontend_impl : visual_frontend
		{
			friend config_dialog;

			frontend_impl(HWND wnd, CSize client_size, visual_frontend_callback& callback, visual_frontend_config& conf);
			virtual void clear();
			virtual void draw();
			virtual void present();
			virtual void on_state_changed(state s);
			virtual void show_configuration(CWindow parent);

		private: // Update
			void update_effect_colors();
			void update_effect_cursor();
			void update_replaygain();
			void update_data();
			void update_size();
			void update_orientation();
			void update_shade_played();

		private: // Misc state
			CComPtr<IDirect3D9> d3d;
			CComPtr<IDirect3DDevice9> dev;

			std::map<unsigned, CComPtr<IDirect3DTexture9>> channel_textures;
			std::deque<CComPtr<IDirect3DTexture9>> annotation_textures;
			std::vector<unsigned> channel_numbers;
			std::vector<channel_info> channel_order;
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

		private: // Host references
			visual_frontend_callback& callback;
			visual_frontend_config& conf;

		private: // Resources
			CComPtr<IDirect3DTexture9> create_waveform_texture();
			void create_vertex_resources();
			void release_vertex_resources();
			void create_default_resources();
			void release_default_resources();

			bool device_still_lost();

			seekbar_state state_copy;

			bool device_lost;
			UINT mip_count;
			D3DFORMAT texture_format;
			bool floating_point_texture;

		private: // Configuration
			scoped_ptr<config_dialog> config;

			void get_effect_compiler(service_ptr_t<effect_compiler>& out);
			void set_effect(service_ptr_t<effect_handle> effect, bool permanent);
		};

		struct config_dialog : CDialogImpl<config_dialog>
		{
			enum { IDD = IDD_CONFIG_D3D, WM_USER_CLEAR_EFFECT_SELECTION = WM_USER + 0x1 };
			config_dialog(weak_ptr<frontend_impl> fe);

			BEGIN_MSG_MAP_EX(config_dialog)
				MSG_WM_INITDIALOG(on_wm_init_dialog)
				MSG_WM_CLOSE(on_wm_close)
				COMMAND_HANDLER_EX(IDC_EFFECT_APPLY, BN_CLICKED, on_effect_apply_click)
				MESSAGE_HANDLER(WM_USER_CLEAR_EFFECT_SELECTION, on_clear_effect_selection)
				COMMAND_HANDLER_EX(IDC_EFFECT_SOURCE, EN_CHANGE, on_effect_source_change)
			END_MSG_MAP()

		private:
			LRESULT on_wm_init_dialog(CWindow focus, LPARAM lparam);
			void on_wm_close();
			void on_effect_apply_click(UINT, int, CWindow);
			LRESULT on_clear_effect_selection(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
			void on_effect_source_change(UINT, int, CEdit);

			virtual void OnFinalMessage(HWND);

			weak_ptr<frontend_impl> fe;
			service_ptr_t<effect_compiler> compiler;
			CFont mono_font;
		};

		struct NOVTABLE effect_compiler : service_base
		{
			struct diagnostic_entry
			{
				struct location
				{
					int row, col;
				};
				location loc;
				std::string type;
				std::string code;
				std::string message;
			};

			virtual ~effect_compiler() {}
			virtual bool compile_fragment(service_ptr_t<effect_handle>& effect, pfc::list_t<diagnostic_entry>& output, pfc::string const& data) = 0;
		};

		struct NOVTABLE effect_handle : service_base
		{
			virtual ~effect_handle();
		};
	}
}

namespace wave
{
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

BOOST_FUSION_ADAPT_STRUCT(
	wave::direct3d9::effect_compiler::diagnostic_entry::location,
	(int, row)
	(int, col)
)

BOOST_FUSION_ADAPT_STRUCT(
	wave::direct3d9::effect_compiler::diagnostic_entry,
	(wave::direct3d9::effect_compiler::diagnostic_entry::location, loc)
	(std::string, type)
	(std::string, code)
	(std::string, message)
)