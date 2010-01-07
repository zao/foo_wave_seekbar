#pragma once

namespace wave
{
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
}

BOOST_CLASS_VERSION(wave::persistent_settings, 6)