//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarWindow.h"
//#include "Direct3D9.h"
//#include "Direct2D.h"
#include "GdiFallback.h"
#include "waveform_sdk/WaveformImpl.h"
#include "FrontendLoader.h"

#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
namespace pt = boost::property_tree;

// {EBEABA3F-7A8E-4A54-A902-3DCF716E6A97}
extern const GUID guid_seekbar_branch;

// {F76A694E-CB85-45A6-A9C6-269877A0AAA4}
static const GUID guid_presentation_scale = 
{ 0xf76a694e, 0xcb85, 0x45a6, { 0xa9, 0xc6, 0x26, 0x98, 0x77, 0xa0, 0xaa, 0xa4 } };

static advconfig_integer_factory g_presentation_scale("Percentage of base display rate to display at (1-400%)", guid_presentation_scale, guid_seekbar_branch, 0.0, 100, 1, 400);

namespace wave
{
	seekbar_window::seekbar_window()
		: placeholder_waveform(make_placeholder_waveform()), fe(new frontend_data), initializing_graphics(false)
		, drag_state(MouseDragNone), possible_next_enqueued(false), repaint_timer_id(0)
	{
	}

	seekbar_window::~seekbar_window()
	{
	}

	void seekbar_window::toggle_orientation(frontend_callback_impl& cb, persistent_settings& s)
	{
		config::orientation o = config::orientation_horizontal;
		if (cb.get_orientation() == o)
			o = config::orientation_vertical;

		cb.set_orientation(o);
	}

	void seekbar_window::apply_settings()
	{
		auto& cb = *fe->callback;
		for (size_t i = 0; i < config::color_count; ++i)
		{
			cb.set_color((config::color)i, settings.override_colors[i]
				? settings.colors[i]
				: global_colors[i]
				);
		}
		cb.set_shade_played(settings.shade_played);
		cb.set_display_mode(settings.display_mode);
		cb.set_downmix_display(settings.downmix_display);
		cb.set_flip_display(settings.flip_display);
		pfc::list_t<channel_info> infos;
		for (size_t i = 0; i < settings.channel_order.size(); ++i)
		{			
			auto const& p = settings.channel_order[i];
			channel_info info = { p.first, p.second };
			infos.add_item(info);
		}
		cb.set_channel_infos(infos.get_ptr(), infos.get_count());
	}

	ref_ptr<visual_frontend> frontend_module::instantiate(config::frontend id, HWND wnd, wave::size size, visual_frontend_callback& callback, visual_frontend_config& conf)
	{
		ref_ptr<visual_frontend> ret;
		if (id == entry->id())
			ret = entry->create(wnd, size, callback, conf);
		return ret;
	}

	ref_ptr<visual_frontend> seekbar_window::create_frontend(config::frontend id)
	{
		ref_ptr<visual_frontend> ret;
		auto sz = client_rect.Size();
		auto modules = list_frontend_modules();
		for (auto I = modules.begin(); I != modules.end(); ++I)
		{
			auto module = *I;
			ret = module->instantiate(id, *this, wave::size(sz.cx, sz.cy), *fe->callback, *fe->conf);
			if (ret)
				return ret;
		}
		return ret;
	}

	void seekbar_window::initialize_frontend()
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		present_scale = g_presentation_scale.get() / 100.0; // ugly, but more explanatory
		present_interval = 100;
		try
		{
			OSVERSIONINFOEX osv = {};
			osv.dwOSVersionInfoSize = sizeof(osv);
#pragma warning(push)
#pragma warning(disable: 4996)
			GetVersionEx((OSVERSIONINFO*)&osv);
#pragma warning(pop)
			bool vista_least_sp1 = (osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0 && osv.wServicePackMajor >= 1);
			bool seven_and_up = (osv.dwMajorVersion == 6 && osv.dwMinorVersion >= 1) || (osv.dwMajorVersion >= 7);

			apply_settings();
			
			bool dynamic_frontend = false;
			switch (settings.active_frontend_kind)
			{
			case config::frontend_direct3d9:
				console::info("Seekbar: taking Direct3D9 path.");
				dynamic_frontend = true;
				break;
			case config::frontend_direct2d1:
				console::info("Seekbar: taking Direct2D1 path.");
				dynamic_frontend = true;
				break;
			case config::frontend_gdi:
				console::info("Seekbar: taking GDI path.");
				dynamic_frontend = true;
				present_interval = 50;
				break;
			default:
				throw std::runtime_error("invalid frontend stored");
			}
			if (dynamic_frontend)
			{
				fe->frontend = create_frontend(settings.active_frontend_kind);
				if (!fe->frontend)
				{
					throw std::runtime_error("unavailable frontend");
				}
				present_interval = fe->frontend->get_present_interval();
			}

			console::info("Seekbar: Frontend initialized.");
			initializing_graphics = true;
		}
		catch (std::exception& e)
		{
			//TODO: Show fall-back help frontend
			initializing_graphics = true;
			console::complain("Seekbar: frontend creation failed", e);
			settings.active_frontend_kind = config::frontend_gdi;
			console::info("Seekbar: taking GDI path.");
			fe->frontend = create_frontend(settings.active_frontend_kind);
			present_interval = 50;
		}

		if (fe->frontend)
		{
			if (repaint_timer_id) {
				KillTimer(repaint_timer_id);
				repaint_timer_id = 0;
			}
			try_get_data();
			fe->frontend->on_state_changed((visual_frontend::state)~0);
			static_api_ptr_t<playback_control> pc;
			if (pc->is_playing()) {
				repaint_timer_id = SetTimer(REPAINT_TIMER_ID, (DWORD)(present_interval / present_scale));
			}
		}
	}

	void seekbar_window::set_cursor_position(float f)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_playback_position(f);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state_position);
	}

	void seekbar_window::set_cursor_visibility(bool b)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_cursor_visible(b);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state_position);
	}

	// time from window coordinates
	double seekbar_window::compute_position(CPoint point)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		double track_length = fe->callback->get_track_length();
		bool horizontal = fe->callback->get_orientation() == config::orientation_horizontal;
		double position = horizontal
			? point.x * track_length / client_rect.Width()
			: point.y * track_length / client_rect.Height()
			;
		if (fe->callback->get_flip_display())
			position = track_length - position;
		
		return std::max(0.0, std::min(track_length, position));
	}

	void seekbar_window::set_seek_position(CPoint point)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		auto position = compute_position(point);

		for each(auto cb in seek_callbacks)
			if (auto p = cb.lock())
				p->on_seek_position(position, fe->callback->is_seeking());

		fe->callback->set_seek_position(position);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state_position);
	}

	void seekbar_window::set_playback_time(double t)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->callback->set_playback_position(t);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state_position);
	}

	void waveform_completion_handler(std::shared_ptr<frontend_data> fe, service_ptr_t<waveform_query> q, uint32_t serial)
	{
		// TODO(zao): Migrate these to new query interface.
		/*
		{
			boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
			if (serial != fe->auto_get_serial)
				return;
			if (fe->valid_buckets/2048.0f >= q->get_progress())
				return;
			fe->valid_buckets = (unsigned)(q->get_progress() * 2048);
			fe->pending_response = *response;
			fe->pending_serial = serial;
		}
		in_main_thread([fe]()
		{
			boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
			if (fe->pending_serial != fe->auto_get_serial)
				return;
			if (fe->callback)
				fe->callback->set_waveform(fe->pending_response.waveform);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_data);
		});
		*/
	}

	void seekbar_window::try_get_data()
	{
		try
		{
			if (core_api::are_services_available())
			{
				boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
				playable_location_impl loc;
				fe->callback->get_playable_location(loc);

				uint32_t next_serial = ++fe->auto_get_serial;
				fe->valid_buckets = 0;
				auto fed = fe;
				auto cb = [fed, next_serial](service_ptr_t<waveform_query> q)
				{
					waveform_completion_handler(fed, q, next_serial);
				};
				auto urgency = waveform_query::needed_urgency;
				auto forced = waveform_query::unforced_query;

				static_api_ptr_t<cache> c;
				auto q = c->create_callback_query(loc, urgency, forced, cb);
				fe->pending_playback_query = q;
				c->get_waveform(q);
			}
		}
		catch (exception_service_not_found&)
		{}
		catch (exception_service_duplicated&)
		{}
	}

	void seekbar_window::save_settings(persistent_settings const& settings, std::vector<char>& out)
	{
		out.clear();
		std::ostringstream os;
		{
			pt::ptree out_pt;
			settings.to_ptree(out_pt);
			write_info(os, out_pt);
		}

		std::string s = os.str();
		std::copy(s.begin(), s.end(), std::back_inserter(out));
	}

	void seekbar_window::load_settings(persistent_settings& settings, std::vector<char> const& in)
	{
		std::string s(in.begin(), in.end());
		
		if (!s.empty() && s[0] == '<') // parsing legacy information
		{
			read_s11n_xml(s, settings);
		}
		else
		{
			pt::ptree config_tree;
			std::istringstream is(s);
			pt::read_info(is, config_tree);
			settings.from_ptree(config_tree);
		}
	}

	void seekbar_window::flush_frontend()
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		fe->frontend.reset();
		initializing_graphics = false;
		if (repaint_timer_id)
			KillTimer(repaint_timer_id);
		repaint_timer_id = 0;
		if (*this)
			Invalidate(FALSE);
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
		if (!m_hWnd) {
			deferred_init.push_back(std::bind(&seekbar_window::set_color, this, which, what, override));
			return;
		}
		if (override)
			settings.colors[which] = what;
		else
			global_colors[which] = what;
		if (settings.override_colors[which] == override)
		{
			boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
			fe->callback->set_color(which, what);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_color);
		}
	}

	void seekbar_window::set_color_override(config::color which, bool override)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		settings.override_colors[which] = override;
		fe->callback->set_color(which, override
			? settings.colors[which]
			: global_colors[which]
			);
		if (fe->frontend)
			fe->frontend->on_state_changed(visual_frontend::state_color);
	}

	void seekbar_window::set_frontend(config::frontend kind)
	{
		settings.active_frontend_kind = kind;
		flush_frontend();
	}

	void seekbar_window::set_orientation(config::orientation o)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		if (fe->callback->get_orientation() != o)
		{
			fe->callback->set_orientation(o);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_orientation);
		}
	}

	void seekbar_window::set_shade_played(bool shade)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		settings.shade_played = shade;
		if (fe->callback->get_shade_played() != shade)
		{
			fe->callback->set_shade_played(shade);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_shade_played);
		}
	}

	void seekbar_window::set_display_mode(config::display_mode mode)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		settings.display_mode = mode;
		if (fe->callback->get_display_mode() != mode)
		{
			fe->callback->set_display_mode(mode);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_display_mode);
		}
	}

	void seekbar_window::set_downmix_display(config::downmix downmix)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		settings.downmix_display = downmix;
		if (fe->callback->get_downmix_display() != downmix)
		{
			fe->callback->set_downmix_display(downmix);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_downmix_display);
		}
	}
	
	void seekbar_window::set_flip_display(bool flip)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		settings.flip_display = flip;
		if (fe->callback->get_flip_display() != flip)
		{
			fe->callback->set_flip_display(flip);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_flip_display);
		}
	}

	void seekbar_window::set_channel_enabled(int ch, bool state)
	{
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		auto& order = settings.channel_order;
		typedef decltype(order[0]) value_type;
		auto I = std::find_if(order.begin(), order.end(), [ch](value_type const& a)
		{
			return a.first == ch;
		});
		if (I != order.end())
		{
			I->second = state;
			apply_settings();
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_channel_order);
		}
	}

	void seekbar_window::swap_channel_order(int ch1, int ch2)
	{
		if (ch1 == ch2)
			return;
		boost::unique_lock<boost::recursive_mutex> lk(fe->mutex);
		auto& order = settings.channel_order;
		typedef decltype(order[0]) value_type;
		auto I1 = std::find_if(order.begin(), order.end(), [ch1](value_type const& a)
		{
			return a.first == ch1;
		});
		auto I2 = std::find_if(order.begin(), order.end(), [ch2](value_type const& a)
		{
			return a.first == ch2;
		});
		if (I1 != order.end() && I2 != order.end())
		{
			std::swap(*I1, *I2);
			apply_settings();
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_channel_order);
		}
	}
}
