//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "Helpers.h"
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>

namespace boost
{
namespace serialization
{
	template<class Archive>
	void serialize(Archive & ar, GUID & g, const unsigned int version)
	{
		ar & make_nvp("Data1", g.Data1);
		ar & make_nvp("Data2", g.Data2);
		ar & make_nvp("Data3", g.Data3);
		ar & make_nvp("Data4", g.Data4);
	}
}
}

namespace wave
{
	struct persistent_settings
	{
		persistent_settings()
			: active_frontend_kind(config::frontend_direct3d9), has_border(true), shade_played(true)
			, display_mode(config::display_normal), flip_display(false), downmix_display(false)
			, generic_strings(&less_guid)
		{
			std::fill_n(colors.begin(), colors.size(), color());
			std::fill_n(override_colors.begin(), override_colors.size(), false);
			channel_order = map_list_of
				(audio_chunk::channel_back_left, true)
				(audio_chunk::channel_front_left, true)
				(audio_chunk::channel_front_center, true)
				(audio_chunk::channel_front_right, true)
				(audio_chunk::channel_back_right, true)
				(audio_chunk::channel_lfe, true);
			insert_remaining_channels();
		}

		config::frontend active_frontend_kind;
		bool has_border;
		boost::array<color, config::color_count> colors;
		boost::array<bool, config::color_count> override_colors;
		bool shade_played;
		config::display_mode display_mode;
		bool flip_display;
		bool downmix_display;
		std::vector<std::pair<int, bool>> channel_order; // int is unnamed channel enum from audio_chunk, contains the channels used
		std::map<GUID, std::string, decltype(&less_guid)> generic_strings;
		

		template <class Archive>
		void save(Archive& ar, const unsigned int version) const
		{
			ar & BOOST_SERIALIZATION_NVP(active_frontend_kind);
			ar & BOOST_SERIALIZATION_NVP(has_border);
			ar & BOOST_SERIALIZATION_NVP(colors);
			ar & BOOST_SERIALIZATION_NVP(override_colors);
			ar & BOOST_SERIALIZATION_NVP(shade_played);
			ar & BOOST_SERIALIZATION_NVP(display_mode);
			ar & BOOST_SERIALIZATION_NVP(downmix_display);
			ar & BOOST_SERIALIZATION_NVP(channel_order);
			ar & BOOST_SERIALIZATION_NVP(generic_strings);
			ar & BOOST_SERIALIZATION_NVP(flip_display);
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
			if (version >= 7)
			{
				ar & BOOST_SERIALIZATION_NVP(display_mode);
				ar & BOOST_SERIALIZATION_NVP(downmix_display);
			}
			if (version >= 8 && version < 9)
			{
				std::vector<int> used_channels;
				ar & BOOST_SERIALIZATION_NVP(used_channels);
			}
			if (version >= 9)
			{
				ar & BOOST_SERIALIZATION_NVP(channel_order);
				insert_remaining_channels();
			}
			if (version >= 10)
			{
				ar & BOOST_SERIALIZATION_NVP(generic_strings);
			}
			if (version >= 11)
			{
				ar & BOOST_SERIALIZATION_NVP(flip_display);
			}
		}
		BOOST_SERIALIZATION_SPLIT_MEMBER()

		void insert_remaining_channels()
		{
			std::set<int> all_channels = list_of
				(audio_chunk::channel_back_left)
				(audio_chunk::channel_front_left)
				(audio_chunk::channel_front_center)
				(audio_chunk::channel_front_right)
				(audio_chunk::channel_back_right)
				(audio_chunk::channel_lfe)
				(audio_chunk::channel_front_center_left)
				(audio_chunk::channel_front_center_right)
				(audio_chunk::channel_back_center)
				(audio_chunk::channel_side_left)
				(audio_chunk::channel_side_right)
				(audio_chunk::channel_top_center)
				(audio_chunk::channel_top_front_left)
				(audio_chunk::channel_top_front_center)
				(audio_chunk::channel_top_front_right)
				(audio_chunk::channel_top_back_left)
				(audio_chunk::channel_top_back_center)
				(audio_chunk::channel_top_back_right);
			for each(int ch in all_channels)
			{
				if (std::find_if(channel_order.begin(), channel_order.end(), [ch](decltype(channel_order[0]) const& a) { return a.first == ch; }) == channel_order.end())
					channel_order.push_back(std::make_pair(ch, false));
			}
		}
	};
}

BOOST_CLASS_VERSION(wave::persistent_settings, 11)
