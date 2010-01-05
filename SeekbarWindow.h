#pragma once
#include "SeekbarState.h"
#include "VisualFrontend.h"
#include "resource.h"

DWORD xbgr_to_argb(COLORREF c, BYTE a = 0xFFU);

namespace wave
{
	struct frontend_callback_impl : visual_frontend_callback, visual_frontend_callback_setter {
		// Getters
		virtual double get_track_length() const { return track_length; }
		virtual double get_playback_position() const { return playback_position; }
		virtual bool is_cursor_visible() const { return cursor_visible; }
		virtual bool is_seeking() const { return seeking; }
		virtual double get_seek_position() const { return seek_position; }
		virtual float get_replaygain(replaygain_value e) const {
			switch(e) {
				case replaygain_album_gain: return rg_album_gain;
				case replaygain_track_gain: return rg_track_gain;
				case replaygain_album_peak: return rg_album_peak;
				case replaygain_track_peak: return rg_track_peak;
				default: return 0.f;
			}
		}
		virtual bool get_playable_location(playable_location& loc) const {
			loc.set_path(location.get_path());
			loc.set_subsong(location.get_subsong());
			return true;
		}
		virtual bool get_waveform(service_ptr_t<waveform>& w) const { if (wf.is_valid()) w = wf; return wf.is_valid(); }
		virtual color get_color(config::color e) const {
			switch(e) {
				case config::color_background: return background_color;
				case config::color_foreground: return text_color;
				case config::color_highlight: return highlight_color;
				case config::color_selection: return selection_color;
				default: return color(1.0f, 105/255.f, 180/255.f);
			}
		}
		virtual CSize get_size() const { return size; }
		virtual config::orientation get_orientation() const { return orientation; }
		virtual bool get_shade_played() const { return shade_played; }

		// Setters
		virtual void set_track_length(double v) { track_length = v; }
		virtual void set_playback_position(double v) { playback_position = v; }
		virtual void set_cursor_visible(bool t) { cursor_visible = t; }
		virtual void set_seeking(bool t) { seeking = t; }
		virtual void set_seek_position(double v) { seek_position = v; }
		virtual void set_replaygain(replaygain_value e, float v) {
			switch (e) {
				case replaygain_album_gain: rg_album_gain = v; break;
				case replaygain_track_gain: rg_track_gain = v; break;
				case replaygain_album_peak: rg_album_peak = v; break;
				case replaygain_track_peak: rg_track_peak = v; break;
			}
		}
		virtual void set_playable_location(playable_location const& loc) {
			location.set_path(loc.get_path());
			location.set_subsong(loc.get_subsong());
		}
		virtual void set_waveform(service_ptr_t<waveform> const& w) { wf = w; }
		virtual void set_color(config::color e, color c) {
			switch(e) {
				case config::color_background: background_color = c; break;
				case config::color_foreground: text_color = c; break;
				case config::color_highlight: highlight_color = c; break;
				case config::color_selection: selection_color = c; break;
			}
		}
		virtual void set_size(CSize s) { size = s; }
		virtual void set_orientation(config::orientation o) { orientation = o; }
		virtual void set_shade_played(bool b) { shade_played = b; }

		double track_length;
		double playback_position;
		bool cursor_visible;
		bool seeking;
		double seek_position;
		float rg_album_gain, rg_track_gain, rg_album_peak, rg_track_peak;
		playable_location_impl location;
		service_ptr_t<waveform> wf;
		color background_color, highlight_color, selection_color, text_color;
		CSize size;
		config::orientation orientation;
		bool shade_played;

		frontend_callback_impl()
			: track_length(1.0), playback_position(0.0), cursor_visible(false), seeking(false)
			, seek_position(0.0), rg_album_gain(0.0), rg_track_gain(0.0), rg_album_peak(0.0), rg_track_peak(0.0)
			, orientation(config::orientation_horizontal), shade_played(true)
		{}
	};

	struct seekbar_window : CFrameWindowImpl<seekbar_window>, play_callback_impl_base, playlist_callback_impl_base, noncopyable
	{
		seekbar_window();
		~seekbar_window();

		DECLARE_WND_CLASS_EX(L"seekbar_dui", CS_HREDRAW | CS_VREDRAW, 0)

		BEGIN_MSG_MAP(seekbar_window)
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

		struct persistent_settings
		{
			persistent_settings()
			: active_frontend_kind(config::frontend_direct3d9), has_border(true), shade_played(true)
			{
				std::fill_n(colors.begin(), colors.size(), color());
				std::fill_n(override_colors.begin(), override_colors.size(), false);
			}

			config::frontend active_frontend_kind;
			bool has_border;
			boost::array<color, config::color_count> colors;
			boost::array<bool, config::color_count> override_colors;
			bool shade_played;
			
			template <class Archive>
			void save(Archive& ar, const unsigned int version) const
			{
				ar & BOOST_SERIALIZATION_NVP(active_frontend_kind);
				ar & BOOST_SERIALIZATION_NVP(has_border);
				ar & BOOST_SERIALIZATION_NVP(colors);
				ar & BOOST_SERIALIZATION_NVP(override_colors);
				ar & BOOST_SERIALIZATION_NVP(shade_played);
			}

			template <class Archive>
			void load(Archive& ar, const unsigned int version)
			{
				ar & BOOST_SERIALIZATION_NVP(active_frontend_kind);
				if (version >= 1 && version < 5)
				{
					config::orientation current_orientation;
					ar & BOOST_SERIALIZATION_NVP(current_orientation);
				}
				if (version >= 2)
					ar & BOOST_SERIALIZATION_NVP(has_border);
				if (version >= 3)
					ar & BOOST_SERIALIZATION_NVP(colors);
				if (version >= 4)
					ar & BOOST_SERIALIZATION_NVP(override_colors);
				if (version >= 6)
					ar & BOOST_SERIALIZATION_NVP(shade_played);
			}
			BOOST_SERIALIZATION_SPLIT_MEMBER()
		};

		persistent_settings settings;
		static void load_settings(persistent_settings& s, std::vector<char> const& in);
		static void save_settings(persistent_settings const& s, std::vector<char>& out);

		void toggle_orientation(frontend_callback_impl& cb, persistent_settings& s);
		virtual bool forward_rightclick() { return false; }

		service_ptr_t<waveform> placeholder_waveform;

		scoped_ptr<frontend_callback_impl> frontend_callback;
		scoped_ptr<struct visual_frontend> frontend;
		bool initializing_graphics;
		bool seek_in_progress;
		bool possible_next_enqueued;
		seekbar_state state;
		color global_colors[config::color_count];

		bool try_get_data();
		void flush_frontend();
		void repaint();

		CRect client_rect;
		UINT_PTR repaint_timer_id;

	private:
		void clean_up();
		void test_playback_order(t_size order);

	public:
		void on_items_added(t_size p_playlist,t_size p_start, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection) {}
		void on_items_reordered(t_size p_playlist,const t_size * p_order,t_size p_count) {}
		void on_items_removing(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
		void on_items_removed(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
		void on_items_selection_change(t_size p_playlist,const bit_array & p_affected,const bit_array & p_state) {}
		void on_item_focus_change(t_size p_playlist,t_size p_from,t_size p_to) {}
		
		void on_items_modified(t_size p_playlist,const bit_array & p_mask) {}
		void on_items_modified_fromplayback(t_size p_playlist,const bit_array & p_mask,play_control::t_display_level p_level) {}

		void on_items_replaced(t_size p_playlist,const bit_array & p_mask,const pfc::list_base_const_t<t_on_items_replaced_entry> & p_data) {}

		void on_item_ensure_visible(t_size p_playlist,t_size p_idx) {}

		void on_playlist_activate(t_size p_old,t_size p_new) {}
		void on_playlist_created(t_size p_index,const char * p_name,t_size p_name_len) {}
		void on_playlists_reorder(const t_size * p_order,t_size p_count) {}
		void on_playlists_removing(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
		void on_playlists_removed(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
		void on_playlist_renamed(t_size p_index,const char * p_new_name,t_size p_new_name_len) {}

		void on_default_format_changed() {}
		void on_playback_order_changed(t_size p_new_index);
		void on_playlist_locked(t_size p_playlist,bool p_locked) {}

	protected:
		void set_border_visibility(bool);
		void set_color(config::color which, color what, bool override);
		void set_color_override(config::color which, bool override);
		void set_frontend(config::frontend kind);
		void set_orientation(config::orientation);
		void set_shade_played(bool);

		struct configuration_dialog : ATL::CDialogImpl<configuration_dialog>
		{
			enum { IDD = IDD_CONFIG };

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
			END_MSG_MAP()

			LRESULT on_wm_init_dialog(CWindow focus, LPARAM lparam);
			void on_wm_close();
			HBRUSH on_wm_ctl_color_static(CDCHandle dc, CWindow wnd);
			void on_frontend_select(UINT code, int id, CWindow control);
			void on_no_border_click(UINT code, int id, CWindow control);
			void on_shade_played_click(UINT code, int id, CWindow control);
			void on_color_click(UINT code, int id, CWindow control);
			void on_use_color_click(UINT code, int id, CWindow control);

			virtual void OnFinalMessage(HWND);

			explicit configuration_dialog(seekbar_window& sw);

		private:
			seekbar_window& sw;

			struct color_info : noncopyable
			{
				CStatic box;
				CBrush brush;
				color color;
				UINT display_id;
				UINT use_id;
			};

			void mk_color_info(config::color color, UINT display_id, UINT use_id);

			color_info colors[config::color_count];
		};
		scoped_ptr<configuration_dialog> config_dialog;
	};
}

BOOST_CLASS_VERSION(wave::seekbar_window::persistent_settings, 6)