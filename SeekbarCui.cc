#include "PchSeekbar.h"
#include "SeekbarCui.h"
#include "../foo_wave_cache/Cache.h"
#include "Direct3D.h"

namespace wave
{
	seekbar_uie::seekbar_uie()
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

	seekbar_uie::~seekbar_uie()
	{
		try
		{
			static_api_ptr_t<cui::colours::manager> cm;
			cm->deregister_common_callback(&color_cb);
		}
		catch (exception_service_not_found&)
		{}
	}

	const GUID& seekbar_uie::get_extension_guid() const
	{
		return s_extension_guid;
	}

	void seekbar_uie::get_name(pfc::string_base& out) const
	{
		out.set_string("Waveform seekbar");
	}

	void seekbar_uie::get_category(pfc::string_base& out) const
	{
		out.set_string("Panels");
	}

	unsigned seekbar_uie::get_type() const
	{
		return uie::type_panel;
	}

	seekbar_uie::color_callback::color_callback(seekbar_uie& parent)
	: sb(parent)
	{}

	void seekbar_uie::color_callback::on_colour_changed(t_size mask) const
	{
		using namespace cui::colours;
		GUID nil = {};
		helper h = helper(nil);
		
		sb.set_color(config::color_background, xbgr_to_color(h.get_colour(colour_background)), false);
		sb.set_color(config::color_foreground, xbgr_to_color(h.get_colour(colour_text)), false);
		sb.set_color(config::color_highlight, xbgr_to_color(h.get_colour(colour_selection_text)), false);
		sb.set_color(config::color_selection, xbgr_to_color(h.get_colour(colour_selection_background)), false);
	}

	void seekbar_uie::color_callback::on_bool_changed(t_size mask) const
	{}

	// {95DF3A44-A2FD-4592-9643-73B40FC7AE57}
	const GUID seekbar_uie::s_extension_guid = 
	{ 0x95df3a44, 0xa2fd, 0x4592, { 0x96, 0x43, 0x73, 0xb4, 0xf, 0xc7, 0xae, 0x57 } };

	void seekbar_uie::set_config(stream_reader * p_reader, t_size p_size, abort_callback & p_abort)
	{
		try
		{
			std::vector<char> v(p_size);
			p_reader->read(&v[0], p_size, p_abort);
			load_settings(settings, v);
			frontend.reset();
		}
		catch (std::exception&)
		{}
	}

	void seekbar_uie::get_config(stream_writer * p_writer, abort_callback & p_abort) const
	{
		std::vector<char> v;
		save_settings(settings, v);
		p_writer->write(&v[0], v.size(), p_abort);
	}
}