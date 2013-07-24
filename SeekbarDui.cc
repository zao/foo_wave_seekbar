//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarDui.h"
#include "Cache.h"
#include <strstream>

namespace wave
{
	seekbar_dui::seekbar_dui(ui_element_config::ptr cfg, ui_element_instance_callback::ptr callback)
	: callback(callback)
	{
		if (core_api::are_services_available())
		{
			get_colors();
		}
		set_configuration(cfg);
	}

	seekbar_dui::~seekbar_dui()
	{
	}

	void seekbar_dui::get_colors()
	{
		frontend_data::lock_type lk(fe->mutex);
		set_color(lk, config::color_background, xbgr_to_color(callback->query_std_color(ui_color_background)), false);
		set_color(lk, config::color_foreground, xbgr_to_color(callback->query_std_color(ui_color_text)), false);
		set_color(lk, config::color_highlight, xbgr_to_color(callback->query_std_color(ui_color_highlight)), false);
		set_color(lk, config::color_selection, xbgr_to_color(callback->query_std_color(ui_color_selection)), false);
	}

	void seekbar_dui::notify(GUID const & what, t_size param1, void const * param2, t_size param2Size)
	{
		if (what == ui_element_notify_colors_changed)
		{
			get_colors();
		}
	}

	// {1E53EAAB-1183-44a4-81B1-51435CB600A2}
	GUID const seekbar_dui::s_guid = 
	{ 0x1e53eaab, 0x1183, 0x44a4, { 0x81, 0xb1, 0x51, 0x43, 0x5c, 0xb6, 0x0, 0xa2 } };

	void seekbar_dui::set_configuration( ui_element_config::ptr data )
	{
		uint8_t const* p = (uint8_t const*)data->get_data();
		try
		{
			frontend_data::lock_type lk(fe->mutex);
			load_settings(settings, std::vector<char>(p, p + data->get_data_size()));
			flush_frontend(lk);
		}
		catch (std::exception& ex)
		{
			console::formatter() << "Wave seekbar: configuration snafu - " << ex.what();
		}
	}

	ui_element_config::ptr seekbar_dui::get_configuration()
	{
		std::vector<char> data;
		save_settings(settings, data);
		return ui_element_config::g_create(s_guid, &data[0], data.size());
	}

	ui_element_config::ptr seekbar_dui::g_get_default_configuration()
	{
		persistent_settings s;
		std::vector<char> v;
		save_settings(s, v);
		return ui_element_config::g_create(s_guid, &v[0], v.size());
	}
}
