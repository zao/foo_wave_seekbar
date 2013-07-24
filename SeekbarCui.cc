//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarCui.h"
#include "Cache.h"

namespace wave
{
	seekbar_uie_base::seekbar_uie_base()
	: color_cb(*this)
	{
		try {
			static_api_ptr_t<cui::colours::manager> cm;
			cm->register_common_callback(&color_cb);
			color_cb.on_colour_changed(~0);
		}
		catch (exception_service_not_found&)
		{}
	}

	seekbar_uie_base::~seekbar_uie_base()
	{
		try
		{
			static_api_ptr_t<cui::colours::manager> cm;
			cm->deregister_common_callback(&color_cb);
		}
		catch (exception_service_not_found&)
		{}
	}

	void seekbar_uie_base::get_name(pfc::string_base& out) const
	{
		out.set_string("Waveform seekbar");
	}

	seekbar_uie_base::color_callback::color_callback(seekbar_uie_base& parent)
	: sb(parent)
	{}

	void seekbar_uie_base::color_callback::on_colour_changed(t_size mask) const
	{
		using namespace cui::colours;
		GUID nil = {};
		helper h = helper(nil);
		frontend_data::lock_type lk(sb.fe->mutex);
		
		sb.set_color(lk, config::color_background, xbgr_to_color(h.get_colour(colour_background)), false);
		sb.set_color(lk, config::color_foreground, xbgr_to_color(h.get_colour(colour_text)), false);
		sb.set_color(lk, config::color_highlight, xbgr_to_color(h.get_colour(colour_selection_text)), false);
		sb.set_color(lk, config::color_selection, xbgr_to_color(h.get_colour(colour_selection_background)), false);
	}

	void seekbar_uie_base::color_callback::on_bool_changed(t_size mask) const
	{}

	// {95DF3A44-A2FD-4592-9643-73B40FC7AE57}
	const GUID s_panel_guid = 
	{ 0x95df3a44, 0xa2fd, 0x4592, { 0x96, 0x43, 0x73, 0xb4, 0xf, 0xc7, 0xae, 0x57 } };

	// {104E910A-5483-4B21-A365-E3E349F76B78}
	const GUID s_toolbar_guid = 
	{ 0x104e910a, 0x5483, 0x4b21, { 0xa3, 0x65, 0xe3, 0xe3, 0x49, 0xf7, 0x6b, 0x78 } };


	void seekbar_uie_base::set_config(stream_reader * p_reader, t_size p_size, abort_callback & p_abort)
	{
		try
		{
      if (p_size)
      {
			  std::vector<char> v(p_size);
			  p_reader->read(&v[0], p_size, p_abort);
			  load_settings(settings, v);
      }
			fe->frontend.reset();
		}
		catch (std::exception&)
		{}
	}

	void seekbar_uie_base::get_config(stream_writer * p_writer, abort_callback & p_abort) const
	{
		std::vector<char> v;
		save_settings(settings, v);
		p_writer->write(&v[0], v.size(), p_abort);
	}
}
