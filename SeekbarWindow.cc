#include "PchSeekbar.h"
#include "SeekbarWindow.h"
#include "Direct3D.h"
#include "Direct2D.h"
#include "GdiFallback.h"

namespace wave
{
	struct waveform_placeholder : waveform
	{
		waveform_placeholder()
		{
			minimum.set_size(2048);
			maximum.set_size(2048);
			rms.set_size(2048);
			for (size_t i = 0; i < 2048; ++i)
			{
				minimum[i] = 0.0f;
				maximum[i] = 0.0f;
				rms[i] = 0.0f;
			}
		}

		virtual bool get_field(pfc::string const& what, pfc::list_base_t<float>& out)
		{
			if (pfc::string::g_equals(what, "minimum"))
				return out = minimum, true;
			if (pfc::string::g_equals(what, "maximum"))
				return out = maximum, true;
			if (pfc::string::g_equals(what, "rms"))
				return out = rms, true;
			return false;
		}

	private:
		pfc::list_hybrid_t<float, 2048> minimum, maximum, rms;
	};

	service_ptr_t<waveform> make_placeholder_waveform()
	{
		return new service_impl_t<waveform_placeholder>;
	}

	static const UINT_PTR REPAINT_TIMER_ID = 0x4242;
	seekbar_window::seekbar_window()
		: play_callback_impl_base(play_callback::flag_on_playback_all), playlist_callback_impl_base(playlist_callback::flag_on_playback_order_changed)
		, placeholder_waveform(make_placeholder_waveform()), frontend_callback(new frontend_callback_impl), initializing_graphics(false)
		, seek_in_progress(false), possible_next_enqueued(false), auto_get_serial(0), repaint_timer_id(0)
		
	{
		frontend_callback->set_waveform(placeholder_waveform);
	}

	seekbar_window::~seekbar_window()
	{}

	void seekbar_window::repaint()
	{
		if ((HWND)*this)
			Invalidate();
	}

	void seekbar_window::on_wm_destroy()
	{
		if (repaint_timer_id)
			KillTimer(repaint_timer_id);
		repaint_timer_id = 0;
		frontend.reset();
	}

	LRESULT seekbar_window::on_wm_erasebkgnd(HDC dc)
	{
		return 1;
	}

	void seekbar_window::on_wm_lbuttondown(UINT wparam, CPoint point)
	{
		seek_in_progress = true;
		frontend_callback->set_seeking(true);
		set_seek_position(point);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
		SetCapture();
		repaint();
	}

	void seekbar_window::on_wm_lbuttonup(UINT wparam, CPoint point)
	{
		seek_in_progress = false;
		ReleaseCapture();
		if (frontend_callback->is_seeking())
		{
			frontend_callback->set_seeking(false);
			set_seek_position(point);
			if (frontend)
				frontend->on_state_changed(visual_frontend::state_position);
			repaint();
			static_api_ptr_t<playback_control> pc;
			pc->playback_seek(frontend_callback->get_seek_position());
		}
	}

	struct menu_item_info
	{
		menu_item_info(UINT f_mask, UINT f_type, UINT f_state, LPTSTR dw_type_data, UINT w_id, HMENU h_submenu = 0, HBITMAP hbmp_checked = 0, HBITMAP hbmp_unchecked = 0, ULONG_PTR dw_item_data = 0, HBITMAP hbmp_item = 0)
		{
			MENUITEMINFO mi =
			{
				sizeof(mi), f_mask | MIIM_TYPE | MIIM_STATE | MIIM_ID, f_type, f_state, w_id, h_submenu, hbmp_checked, hbmp_unchecked, dw_item_data, dw_type_data, _tcslen(dw_type_data), hbmp_item
			};
			this->mi = mi;
		}
		mutable MENUITEMINFO mi;
		operator LPMENUITEMINFO () const
		{
			return &mi;
		}
	};

	void seekbar_window::toggle_orientation(frontend_callback_impl& cb, persistent_settings& s)
	{
		config::orientation o = config::orientation_horizontal;
		if (cb.get_orientation() == o)
			o = config::orientation_vertical;

		cb.set_orientation(o);
	}
	
	void seekbar_window::on_wm_rbuttonup(UINT wparam, CPoint point)
	{
		if (forward_rightclick())
		{
			SetMsgHandled(FALSE);
			return;
		}

		WTL::CMenu m;
		m.CreatePopupMenu();
		m.InsertMenu(-1, MF_BYPOSITION | MF_STRING, 3, L"Configure");
		ClientToScreen(&point);
		BOOL ans = m.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, *this, 0);
		config::frontend old_kind = settings.active_frontend_kind;
		switch(ans)
		{
		case 3:
			if (config_dialog)
				config_dialog->BringWindowToTop();
			else
			{
				config_dialog.reset(new configuration_dialog(*this));
				config_dialog->Create(*this);
			}
			break;
		default:
			return;
		}
	}

	bool is_outside(CPoint point, CRect r, int N, bool horizontal)
	{
		if (!horizontal)
		{
			std::swap(point.x, point.y);
			std::swap(r.right, r.bottom);
			std::swap(r.left, r.top);
		}
		return point.y < -2 * N || point.y > r.bottom - r.top + 2 * N ||
		       point.x < -N     || point.x > r.right - r.left + N;
	}

	void seekbar_window::on_wm_mousemove(UINT wparam, CPoint point)
	{
		if (seek_in_progress)
		{
			CRect r;
			GetWindowRect(r);
			int const N = 40;
			bool horizontal = frontend_callback->get_orientation() == config::orientation_horizontal;

			frontend_callback->set_seeking(!is_outside(point, r, N, horizontal));

			set_seek_position(point);
			if (frontend)
				frontend->on_state_changed(visual_frontend::state_position);
			repaint();
		}
	}

	void seekbar_window::on_wm_paint(HDC dc)
	{
		GetClientRect(client_rect);
		if (!(client_rect.right > 1 && client_rect.bottom > 1))
		{
			ValidateRect(0);
			return;
		}

		if (!frontend && !initializing_graphics)
		{
			try
			{
				OSVERSIONINFOEX osv = {};
				osv.dwOSVersionInfoSize = sizeof(osv);
				GetVersionEx((OSVERSIONINFO*)&osv);
				bool vista_least_sp1 = (osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0 && osv.wServicePackMajor >= 1);
				bool seven_and_up = (osv.dwMajorVersion == 6 && osv.dwMinorVersion >= 1) || (osv.dwMajorVersion >= 7);

				for (size_t i = 0; i < config::color_count; ++i)
				{
					frontend_callback->set_color((config::color)i, settings.override_colors[i]
						? settings.colors[i]
						: global_colors[i]
						);
				}
				frontend_callback->set_shade_played(settings.shade_played);
				
				DWORD present_interval = 10;
				switch (settings.active_frontend_kind)
				{
	#if 0
				case persistent_settings::frontend_direct3d10:
					console::info("Seekbar: taking Direct3D10 path.");
					frontend.reset(new direct3d10_frontend(*this, client_rect.Size(), *frontend_callback));
					break;
	#endif
				case config::frontend_direct3d9:
					console::info("Seekbar: taking Direct3D9 path.");
					frontend.reset(new direct3d9_frontend(*this, client_rect.Size(), *frontend_callback));
					break;
				case config::frontend_direct2d1:
					console::info("Seekbar: taking Direct2D1 path.");
					frontend.reset(new direct2d1_frontend(*this, client_rect.Size(), *frontend_callback));
					present_interval = 25;
					break;
				case config::frontend_gdi:
					console::info("Seekbar: taking GDI path.");
					frontend.reset(new gdi_fallback_frontend(*this, client_rect.Size(), *frontend_callback));
					present_interval = 50;
					break;
				}
				console::info("Seekbar: Frontend initialized.");
				initializing_graphics = true;

				try_get_data();

				frontend->on_state_changed((visual_frontend::state)~0);

				if (frontend)
				{
					repaint_timer_id = SetTimer(REPAINT_TIMER_ID, present_interval);
				}
			}
			catch (std::exception& e)
			{
				//TODO: Show fall-back help frontend
				initializing_graphics = true;
				console::complain("Seekbar: frontend creation failed", e);
				settings.active_frontend_kind = config::frontend_gdi;
			}
		}

		if (frontend)
		{
			frontend->clear();
			frontend->draw();
			frontend->present();
		}

		ValidateRect(0);
	}

	void seekbar_window::on_wm_size(UINT wparam, CSize size)
	{
		if (size.cx < 1 || size.cy < 1)
			return;
		set_orientation(size.cx >= size.cy
			? config::orientation_horizontal
			: config::orientation_vertical);
		frontend_callback->set_size(size);
		if (frontend)
			frontend->on_state_changed((visual_frontend::state)(visual_frontend::state_size | visual_frontend::state_orientation));
		repaint();
	}

	void seekbar_window::on_wm_timer(UINT_PTR wparam)
	{
		if (wparam == REPAINT_TIMER_ID && core_api::are_services_available())
		{
			static_api_ptr_t<playback_control> pc;
			double t = pc->playback_get_position();
			set_cursor_position((float)t);
			if (frontend)
				frontend->on_state_changed(visual_frontend::state_position);
			repaint();
		}
	}

	static const GUID order_default = { 0xbfc61179, 0x49ad, 0x4e95, { 0x8d, 0x60, 0xa2, 0x27, 0x06, 0x48, 0x55, 0x05 } };
	static const GUID order_repeat_playlist = { 0x681cc6ea, 0x60ae, 0x4bf9, { 0x91, 0x3b, 0xbb, 0x5f, 0x4e, 0x86, 0x4f, 0x2a } };
	static const GUID order_repeat_track = { 0x4bf4b280, 0x0bb4, 0x4dd0, { 0x8e, 0x84, 0x37, 0xc3, 0x20, 0x9c, 0x3d, 0xa2 } };
	static const GUID order_random = { 0x611af974, 0x4316, 0x43ac, { 0xab, 0xec, 0xc2, 0xea, 0xa3, 0x5c, 0x3f, 0x9b } };
	static const GUID order_shuffle_tracks = { 0xc5cf4a57, 0x8c01, 0x480c, { 0xb3, 0x34, 0x36, 0x19, 0x64, 0x5a, 0xda, 0x8b } };
	static const GUID order_shuffle_albums = { 0x499e0b08, 0xc887, 0x48c1, { 0x9c, 0xca, 0x27, 0x37, 0x7c, 0x8b, 0xfd, 0x30 } };
	static const GUID order_shuffle_folders = { 0x83c37600, 0xd725, 0x4727, { 0xb5, 0x3c, 0xbd, 0xef, 0xfe, 0x5f, 0x8d, 0xc7 } };

	void enqueue(playable_location const& location)
	{
		shared_ptr<get_request> request(new get_request);

		request->location.copy(location);
		request->user_requested = false;
		request->completion_handler = [](shared_ptr<get_response>) {};

		static_api_ptr_t<cache> c;
		c->get_waveform(request);
	}

	void seekbar_window::test_playback_order(t_size playback_order_index)
	{
		if (!core_api::are_services_available())
			return;
		static_api_ptr_t<playlist_manager> pm;

		GUID current_order = pm->playback_order_get_guid(playback_order_index);
		if (current_order == order_default || current_order == order_repeat_playlist || current_order == order_shuffle_albums || current_order == order_shuffle_folders)
		{
			t_size playlist, index;
			if (pm->get_playing_item_location(&playlist, &index))
			{
				t_size count = pm->playlist_get_item_count(playlist);
				metadb_handle_ptr next = pm->playlist_get_item_handle(playlist, (index + 1) % count);
				try
				{
					enqueue(next->get_location());
					possible_next_enqueued = true;
				}
				catch (exception_service_not_found&)
				{}
				catch (exception_service_duplicated&)
				{}
			}
		}
		else
			possible_next_enqueued = true;
	}

	void seekbar_window::on_playback_order_changed(t_size new_index)
	{
		test_playback_order(new_index);
	}

	void seekbar_window::on_playback_starting(playback_control::t_track_command,bool)
	{
	}

	void seekbar_window::on_playback_new_track(metadb_handle_ptr ptr)
	{
		frontend_callback->set_track_length(ptr->get_length());
		file_info_impl info;
		ptr->get_info(info);

		replaygain_info rg = info.get_replaygain();
#define SET_REPLAYGAIN(Name) frontend_callback->set_replaygain(visual_frontend_callback::replaygain_##Name, rg.m_##Name);
		SET_REPLAYGAIN(album_gain)
		SET_REPLAYGAIN(track_gain)
		SET_REPLAYGAIN(album_peak)
		SET_REPLAYGAIN(track_peak)
#undef  SET_REPLAYGAIN

		set_cursor_position(0.0f);
		set_cursor_visibility(true);
		frontend_callback->set_playable_location(ptr->get_location());

		if (frontend)
			frontend->on_state_changed(visual_frontend::state(visual_frontend::state_replaygain | visual_frontend::state_position | visual_frontend::state_track));

		possible_next_enqueued = false;

		try_get_data();
		repaint();
	}

	void seekbar_window::on_playback_stop(playback_control::t_stop_reason)
	{
		set_cursor_visibility(false);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
		repaint();
	}

	void seekbar_window::on_playback_seek(double t)
	{
		set_cursor_position((float)t);
		set_cursor_visibility(true);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
		repaint();
	}

	void seekbar_window::on_playback_pause(bool)
	{
	}

	void seekbar_window::on_playback_edited(metadb_handle_ptr)
	{
	}

	void seekbar_window::on_playback_dynamic_info(const file_info &)
	{
	}

	void seekbar_window::on_playback_dynamic_info_track(const file_info &)
	{
	}

	void seekbar_window::on_playback_time(double t)
	{
		if (t > 1.0 && !possible_next_enqueued)
		{
			static_api_ptr_t<playlist_manager> pm;
			test_playback_order(pm->playback_order_get_active());
		}
		set_cursor_visibility(true);

		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
		repaint();
	}

	void seekbar_window::on_volume_change(float)
	{
	}

	void seekbar_window::set_cursor_position(float f)
	{
		frontend_callback->set_playback_position(f);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
	}

	void seekbar_window::set_cursor_visibility(bool b)
	{
		frontend_callback->set_cursor_visible(b);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
	}

	void seekbar_window::set_seek_position(CPoint point)
	{
		bool horizontal = frontend_callback->get_orientation() == config::orientation_horizontal;
		frontend_callback->set_seek_position(horizontal
			? point.x * frontend_callback->get_track_length() / client_rect.Width()
			: point.y * frontend_callback->get_track_length() / client_rect.Height()
			);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_position);
	}

	void seekbar_window::waveform_completion_handler(shared_ptr<get_response> response, uint32_t serial)
	{
		in_main_thread([this, response, serial]()
		{
			if (serial != auto_get_serial) return;
			frontend_callback->set_waveform(response->waveform);
			if (this->frontend)
				this->frontend->on_state_changed(visual_frontend::state_data);
		});
	}

	void seekbar_window::try_get_data()
	{
		try
		{
			if (core_api::are_services_available())
			{
				shared_ptr<get_request> request(new get_request);
				request->user_requested = false;
				frontend_callback->get_playable_location(request->location);
				uint32_t next_serial = ++auto_get_serial;
				request->completion_handler = [this, next_serial](shared_ptr<get_response> response)
				{
					waveform_completion_handler(response, next_serial);
				};

				static_api_ptr_t<cache> c;
				c->get_waveform(request);
			}
		}
		catch (exception_service_not_found&)
		{}
		catch (exception_service_duplicated&)
		{}
	}

	void seekbar_window::save_settings(persistent_settings const& settings, std::vector<char>& out)
	{
		TRACK_CALL(save_settings);
		scoped_ptr<uCallStackTracker> t;
		out.clear();
		t.reset(new uCallStackTracker("ostream"));
		io::filtering_ostream os(std::back_inserter(out));
		t.reset(new uCallStackTracker("oarchive"));
		boost::archive::xml_oarchive ar(os);
		t.reset(new uCallStackTracker("save"));
		ar & BOOST_SERIALIZATION_NVP(settings);
	}

	void seekbar_window::load_settings(persistent_settings& settings, std::vector<char> const& in)
	{
		TRACK_CALL(load_settings);
		scoped_ptr<uCallStackTracker> t(new uCallStackTracker("istream"));
		io::filtering_istream is(boost::make_iterator_range(in));
		t.reset(new uCallStackTracker("iarchive"));
		boost::archive::xml_iarchive ar(is);
		t.reset(new uCallStackTracker("load"));
		ar & BOOST_SERIALIZATION_NVP(settings);
	}

	void seekbar_window::flush_frontend()
	{
		frontend.reset();
		initializing_graphics = false;
		if (repaint_timer_id)
			KillTimer(repaint_timer_id);
		repaint_timer_id = 0;
	}

	void seekbar_window::set_border_visibility(bool visible)
	{
		if (settings.has_border != visible)
		{
			settings.has_border = visible;
			if (visible)
				ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_FRAMECHANGED);
			else
				ModifyStyleEx(WS_EX_STATICEDGE, 0, SWP_FRAMECHANGED);
		}
	}

	void seekbar_window::set_color(config::color which, color what, bool override)
	{
		if (override)
			settings.colors[which] = what;
		else
			global_colors[which] = what;
		if (settings.override_colors[which] == override)
		{
			frontend_callback->set_color(which, what);
			if (frontend)
				frontend->on_state_changed(visual_frontend::state_color);
		}
	}

	void seekbar_window::set_color_override(config::color which, bool override)
	{
		settings.override_colors[which] = override;
		frontend_callback->set_color(which, override
			? settings.colors[which]
			: global_colors[which]
			);
		if (frontend)
			frontend->on_state_changed(visual_frontend::state_color);
	}

	void seekbar_window::set_frontend(config::frontend kind)
	{
		settings.active_frontend_kind = kind;
		flush_frontend();
	}

	void seekbar_window::set_orientation(config::orientation o)
	{
		if (frontend_callback->get_orientation() != o)
		{
			frontend_callback->set_orientation(o);
			if (frontend)
				frontend->on_state_changed(visual_frontend::state_orientation);
		}
	}

	void seekbar_window::set_shade_played(bool shade)
	{
		settings.shade_played = shade;
		if (frontend_callback->get_shade_played() != shade)
		{
			frontend_callback->set_shade_played(shade);
			if (frontend)
				frontend->on_state_changed(visual_frontend::state_shade_played);
		}
	}

	seekbar_state::seekbar_state()
	{}
}