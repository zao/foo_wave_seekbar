#pragma once
#include "SeekbarState.h"
#include "VisualFrontend.h"
#include "resource.h"
#include "PersistentSettings.h"
#include "FrontendCallbackImpl.h"
#include "FrontendConfigImpl.h"
#include "Helpers.h"
#include "SeekCallback.h"

namespace wave
{
	struct frontend_data
	{
		frontend_data() : auto_get_serial(0) {}
		void clear()
		{
			callback.reset();
			frontend.reset();
		}
		boost::recursive_mutex mutex;
		scoped_ptr<frontend_callback_impl> callback;
		scoped_ptr<frontend_config_impl> conf;
		shared_ptr<visual_frontend> frontend;
		uint32_t auto_get_serial;
	};

	struct seekbar_window : CWindowImpl<seekbar_window>, play_callback_impl_base, playlist_callback_impl_base, noncopyable
	{
		seekbar_window();
		~seekbar_window();

		DECLARE_WND_CLASS_EX(L"seekbar_dui", CS_HREDRAW | CS_VREDRAW, 0)

		BEGIN_MSG_MAP(seekbar_window)
			MSG_WM_DESTROY(on_wm_destroy);
			MSG_WM_ERASEBKGND(on_wm_erasebkgnd)
			MSG_WM_LBUTTONDOWN(on_wm_lbuttondown)
			MSG_WM_LBUTTONUP(on_wm_lbuttonup)
			MSG_WM_RBUTTONUP(on_wm_rbuttonup)
			MSG_WM_MOUSEMOVE(on_wm_mousemove)
			MSG_WM_PAINT(on_wm_paint)
			MSG_WM_SIZE(on_wm_size)
			MSG_WM_TIMER(on_wm_timer)
		END_MSG_MAP()

	private:
		void on_wm_destroy();
		LRESULT on_wm_erasebkgnd(HDC dc);
		void on_wm_lbuttondown(UINT wparam, CPoint point);
		void on_wm_lbuttonup(UINT wparam, CPoint point);
		void on_wm_rbuttonup(UINT wparam, CPoint point);
		void on_wm_mousemove(UINT wparam, CPoint point);
		void on_wm_paint(HDC dc);
		void on_wm_size(UINT wparam, CSize size);
		void on_wm_timer(UINT_PTR wparam);

		void on_playback_starting(playback_control::t_track_command,bool);
		void on_playback_new_track(metadb_handle_ptr);
		void on_playback_stop(playback_control::t_stop_reason);
		void on_playback_seek(double);
		void on_playback_pause(bool);
		void on_playback_edited(metadb_handle_ptr);
		void on_playback_dynamic_info(const file_info &);
		void on_playback_dynamic_info_track(const file_info &);
		void on_playback_time(double);
		void on_volume_change(float);

	protected:
		void set_cursor_position(float f);
		void set_cursor_visibility(bool b);
		void set_seek_position(CPoint point);

		persistent_settings settings;
		static void load_settings(persistent_settings& s, std::vector<char> const& in);
		static void save_settings(persistent_settings const& s, std::vector<char>& out);

		void toggle_orientation(frontend_callback_impl& cb, persistent_settings& s);
		virtual bool forward_rightclick() { return false; }

		service_ptr_t<waveform> placeholder_waveform;

		shared_ptr<frontend_data> fe;

		bool initializing_graphics;
		bool seek_in_progress;
		bool possible_next_enqueued;
		seekbar_state state;
		color global_colors[config::color_count];

		void try_get_data();
		void flush_frontend();
		void repaint();

		CRect client_rect;
		UINT_PTR repaint_timer_id;

		CPoint last_seek_point;

		boost::shared_ptr<seek_callback> tooltip;
		std::vector<boost::weak_ptr<seek_callback>> seek_callbacks;

	private:
		void initialize_frontend();
		void test_playback_order(t_size order);
		void apply_settings();

	public:
		void on_playback_order_changed(t_size p_new_index);

	protected:
		void set_border_visibility(bool);
		void set_color(config::color which, color what, bool override);
		void set_color_override(config::color which, bool override);
		void set_frontend(config::frontend kind);
		void set_orientation(config::orientation);
		void set_shade_played(bool);
		void set_display_mode(config::display_mode);
		void set_downmix_display(bool);
		void set_channel_enabled(int channel, bool);
		void swap_channel_order(int ch1, int ch2);

		// Config dialog
		struct configuration_dialog : ATL::CDialogImpl<configuration_dialog>
		{
			enum { IDD = IDD_CONFIG };
			configuration_dialog();

#define COLOR_CLICK_HANDLER(Name) COMMAND_HANDLER_EX(Name, STN_CLICKED, on_color_click)
#define COLOR_USE_HANDLER(Name) COMMAND_HANDLER_EX(Name, BN_CLICKED, on_use_color_click)

			BEGIN_MSG_MAP_EX(configuration_dialog)
				MSG_WM_INITDIALOG(on_wm_init_dialog)
				MSG_WM_CLOSE(on_wm_close)
				MSG_WM_CTLCOLORSTATIC(on_wm_ctl_color_static)
				COMMAND_HANDLER_EX(IDC_FRONTEND, CBN_SELCHANGE, on_frontend_select)
				COMMAND_HANDLER_EX(IDC_NOBORDER, BN_CLICKED, on_no_border_click)
				COMMAND_HANDLER_EX(IDC_SHADEPLAYED, BN_CLICKED, on_shade_played_click)
				COLOR_CLICK_HANDLER(IDC_COLOR_BACKGROUND)
				COLOR_CLICK_HANDLER(IDC_COLOR_FOREGROUND)
				COLOR_CLICK_HANDLER(IDC_COLOR_HIGHLIGHT)
				COLOR_CLICK_HANDLER(IDC_COLOR_SELECTION)
				COLOR_USE_HANDLER(IDC_USE_BACKGROUND)
				COLOR_USE_HANDLER(IDC_USE_FOREGROUND)
				COLOR_USE_HANDLER(IDC_USE_HIGHLIGHT)
				COLOR_USE_HANDLER(IDC_USE_SELECTION)
				COMMAND_HANDLER_EX(IDC_DISPLAYMODE, CBN_SELCHANGE, on_display_select)
				COMMAND_HANDLER_EX(IDC_DOWNMIX, BN_CLICKED, on_downmix_click)
				NOTIFY_HANDLER_EX(IDC_CHANNELS, LVN_ITEMCHANGED, on_channel_changed)
				NOTIFY_HANDLER_EX(IDC_CHANNELS, NM_CLICK, on_channel_click)
				COMMAND_HANDLER_EX(IDC_CHANNEL_UP, BN_CLICKED, on_channel_up)
				COMMAND_HANDLER_EX(IDC_CHANNEL_DOWN, BN_CLICKED, on_channel_down)
				COMMAND_HANDLER_EX(IDC_CONFIGURE, BN_CLICKED, on_configure_click)
			END_MSG_MAP()

#define HANDLER_EX_IMPL(Name) void Name(UINT, int, CWindow)

			LRESULT on_wm_init_dialog(CWindow focus, LPARAM lparam);
			void on_wm_close();
			HBRUSH on_wm_ctl_color_static(CDCHandle dc, CWindow wnd);
			HANDLER_EX_IMPL(on_frontend_select);
			HANDLER_EX_IMPL(on_no_border_click);
			HANDLER_EX_IMPL(on_shade_played_click);
			HANDLER_EX_IMPL(on_color_click);
			HANDLER_EX_IMPL(on_use_color_click);
			HANDLER_EX_IMPL(on_display_select);
			HANDLER_EX_IMPL(on_downmix_click);
			LRESULT on_channel_changed(NMHDR* nm);
			LRESULT on_channel_click(NMHDR* nm);
			HANDLER_EX_IMPL(on_channel_up);
			HANDLER_EX_IMPL(on_channel_down);	
			HANDLER_EX_IMPL(on_configure_click);

			virtual void OnFinalMessage(HWND);

			explicit configuration_dialog(seekbar_window& sw);

		private:
			seekbar_window& sw;
			bool initializing;

			CListViewCtrl channels;
			struct buttons
			{
				CButton up, down;
			} buttons;

			struct color_info
			{
				CStatic box;
				CBrush brush;
				color color;
				UINT display_id;
				UINT use_id;
			};

			void mk_color_info(config::color color, UINT display_id, UINT use_id);
			color_info colors[config::color_count];

			struct channel_info
			{
				std::wstring text;
				int data;
			};

			void swap_channels(int i1, int i2);
			channel_info get_item(int, CListBox&);
			void add_item(channel_info const&, CListBox&);
			void remove_item(int, CListBox&);
		};
		scoped_ptr<configuration_dialog> config_dialog;
	};
}