#pragma once
#include "../frontend_sdk/VisualFrontend.h"
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

		namespace parameters
		{
			extern std::string const background_color, foreground_color, highlight_color, selection_color,
				cursor_position, cursor_visible,
				seek_position, seeking,
				viewport_size, replaygain,
				orientation, flipped, shade_played,
				waveform_data;
		};

		struct effect_parameters
		{
			typedef boost::variant<float, bool, D3DXVECTOR4, D3DXMATRIX, IDirect3DTexture9*> attribute;
			std::map<std::string, attribute> attributes;

			template <typename T>
			void set(std::string what, T const& t)
			{
				attributes[what] = t;
			}

			void apply_to(CComPtr<ID3DXEffect> fx);
		};

		struct frontend_impl : visual_frontend
		{
			friend config_dialog;

			frontend_impl(HWND wnd, wave::size client_size, visual_frontend_callback& callback, visual_frontend_config& conf);
			virtual void clear();
			virtual void draw();
			virtual void present();
			virtual void on_state_changed(state s);
			virtual void show_configuration(HWND parent);
			virtual void close_configuration();

		private: // Update
			void update_effect_colors();
			void update_effect_cursor();
			void update_replaygain();
			void update_data();
			void update_size();
			void update_orientation();
			void update_flipped();
			void update_shade_played();

		private: // Misc state
			CComPtr<IDirect3D9> d3d;
			CComPtr<IDirect3DDevice9> dev;

			std::map<unsigned, CComPtr<IDirect3DTexture9>> channel_textures;
			std::vector<unsigned> channel_numbers;
			std::vector<channel_info> channel_order;

			std::stack<shared_ptr<effect_handle>> effect_stack;
			shared_ptr<effect_handle> effect_override;

			CComPtr<IDirect3DVertexBuffer9> vb;
			CComPtr<IDirect3DVertexDeclaration9> decl;

			effect_parameters effect_params;

			CComPtr<ID3DXEffect> select_effect();

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

			bool device_lost;
			UINT mip_count;
			D3DFORMAT texture_format;
			bool floating_point_texture;

		private: // Configuration
			scoped_ptr<config_dialog> config;

			void get_effect_compiler(shared_ptr<effect_compiler>& out);
			void set_effect(shared_ptr<effect_handle> effect, bool permanent);
		};

		struct config_dialog : CDialogImpl<config_dialog>
		{
			enum { IDD = IDD_CONFIG_D3D, WM_USER_CLEAR_EFFECT_SELECTION = WM_USER + 0x1 };
			config_dialog(weak_ptr<frontend_impl> fe);

			BEGIN_MSG_MAP_EX(config_dialog)
				MSG_WM_INITDIALOG(on_wm_init_dialog)
				MSG_WM_CLOSE(on_wm_close)
				COMMAND_HANDLER_EX(IDC_EFFECT_APPLY, BN_CLICKED, on_effect_apply_click)
				COMMAND_HANDLER_EX(IDC_EFFECT_DEFAULT, BN_CLICKED, on_effect_default_click)
				COMMAND_HANDLER_EX(IDC_EFFECT_RESET, BN_CLICKED, on_effect_reset_click)
				MESSAGE_HANDLER(WM_USER_CLEAR_EFFECT_SELECTION, on_clear_effect_selection)
				COMMAND_HANDLER_EX(IDC_EFFECT_SOURCE, EN_CHANGE, on_effect_source_change)
			END_MSG_MAP()

		private:
			LRESULT on_wm_init_dialog(CWindow focus, LPARAM lparam);
			void on_wm_close();
			void on_effect_apply_click(UINT, int, CWindow);
			void on_effect_default_click(UINT, int, CWindow);
			void on_effect_reset_click(UINT, int, CWindow);
			LRESULT on_clear_effect_selection(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
			void on_effect_source_change(UINT, int, CEdit);

			virtual void OnFinalMessage(HWND);

			weak_ptr<frontend_impl> fe;
			shared_ptr<effect_compiler> compiler;
			CFont mono_font;
			CEdit code_box, error_box;
			CButton apply_button, default_button, reset_button;
			shared_ptr<effect_handle> fx;
		};

		struct NOVTABLE effect_compiler : noncopyable
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
			virtual bool compile_fragment(shared_ptr<effect_handle>& effect, std::deque<diagnostic_entry>& output, std::string const& data) = 0;
		};

		struct NOVTABLE effect_handle : noncopyable
		{
			virtual ~effect_handle() {}
			virtual CComPtr<ID3DXEffect> get_effect() const = 0;
		};
	}
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