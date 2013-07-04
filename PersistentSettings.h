//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "Helpers.h"
#include "frontend_sdk/VisualFrontend.h"

#include <array>
#include <boost/property_tree/ptree.hpp>

namespace wave
{
	struct persistent_settings
	{
		persistent_settings();

		config::frontend active_frontend_kind;
		bool has_border;
		std::array<color, config::color_count> colors;
		std::array<bool, config::color_count> override_colors;
		bool shade_played;
		config::display_mode display_mode;
		bool flip_display;
		config::downmix downmix_display;
		std::vector<std::pair<int, bool>> channel_order; // int is unnamed channel enum from audio_chunk, contains the channels used
		std::map<GUID, std::string, decltype(&less_guid)> generic_strings;

		void from_ptree(boost::property_tree::ptree const& src);
		void to_ptree(boost::property_tree::ptree& out) const;

		void insert_remaining_channels();
	};

	void read_s11n_xml(std::string xml, persistent_settings& settings);
}

#if defined(SEEKBAR_USE_S11N)
BOOST_CLASS_VERSION(wave::persistent_settings, 11)
#endif
