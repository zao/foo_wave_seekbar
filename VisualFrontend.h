#pragma once
#include "SeekbarState.h"

namespace wave
{
	service_ptr_t<waveform> make_placeholder_waveform();

	namespace config
	{
		enum orientation
		{
			orientation_horizontal,
			orientation_vertical
		};
		enum color
		{
			color_background,
			color_foreground,
			color_highlight,
			color_selection,
			color_count
		};
		enum frontend
		{
			frontend_direct3d9,
			frontend_direct3d10,
			frontend_direct2d1,
			frontend_gdi
		};
		enum display_mode
		{
			display_normal,
			display_positive,
			display_negative,
			display_average,
			display_minimum,
			display_maximum
		};

		namespace strings
		{
			__declspec(selectany) wchar_t const* frontend[] =
			{
				L"Direct3D 9.0c", L"Direct3D 10.0", L"Direct2D 1.0", L"GDI"
			};
			__declspec(selectany) wchar_t const* display_mode[] =
			{
				L"Normal", L"Only + half", L"Only - half", L"Average of +/-", L"Minimum of +/-", L"Maximum of +/-"
			};
			__declspec(selectany) std::map<int, wchar_t const*> channel_names =
			boost::assign::map_list_of
				(audio_chunk::channel_front_left, L"Front left")
				(audio_chunk::channel_front_right, L"Front right")
				(audio_chunk::channel_front_center, L"Front center (mono)")
				(audio_chunk::channel_lfe, L"LFE")
				(audio_chunk::channel_back_left, L"Rear left")
				(audio_chunk::channel_back_right, L"Rear right")
				(audio_chunk::channel_front_center_left, L"Front center left")
				(audio_chunk::channel_front_center_right, L"Front center right")
				(audio_chunk::channel_back_center, L"Back center")
				(audio_chunk::channel_side_left, L"Side left")
				(audio_chunk::channel_side_right, L"Side right")
				(audio_chunk::channel_top_center, L"Top center")
				(audio_chunk::channel_top_front_left, L"Top front left")
				(audio_chunk::channel_top_front_center, L"Top front center")
				(audio_chunk::channel_top_front_right, L"Top front right")
				(audio_chunk::channel_top_back_left, L"Top back left")
				(audio_chunk::channel_top_back_center, L"Top back center")
				(audio_chunk::channel_top_back_right, L"Top back right")
			;
		}
	}

	struct color {
		explicit color(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f) : r(r), g(g), b(b), a(a) {}
		float r, g, b, a;

		template <typename Archive>
		void serialize(Archive& ar, const unsigned version)
		{
			ar & BOOST_SERIALIZATION_NVP(r) & BOOST_SERIALIZATION_NVP(g) & BOOST_SERIALIZATION_NVP(b) & BOOST_SERIALIZATION_NVP(a);
		}
	};

	inline color xbgr_to_color(DWORD c, BYTE a = 0xFFU) {
		float r = GetRValue(c) / 255.f,
		      g = GetGValue(c) / 255.f,
			  b = GetBValue(c) / 255.f;
		return color(r, g, b, a / 255.f);
	}

	inline COLORREF color_to_xbgr(color c)
	{
		return RGB(
			(BYTE)(c.a * c.r * 255),
			(BYTE)(c.a * c.g * 255),
			(BYTE)(c.a * c.b * 255)
			);
	}

	inline COLORREF color_to_abgr(color c)
	{
		color pc(c.r * c.a, c.g * c.a, c.b * c.a, c.a);
		return color_to_xbgr(pc) | (((BYTE)(c.a * 255)) << 24);
	}

	struct channel_info
	{
		int channel;
		bool enabled;
	};

	struct visual_frontend
	{
		virtual ~visual_frontend() {};
		virtual void clear() = 0;
		virtual void draw() = 0;
		virtual void present() = 0;

		enum state {
			state_color = 1<<0,
			state_replaygain = 1<<1,
			state_position = 1<<2,
			state_size = 1<<3,
			state_data = 1<<4,
			state_track = 1<<5,
			state_orientation = 1<<6,
			state_shade_played = 1<<7,
			state_display_mode = 1<<8,
			state_downmix_display = 1<<9,
			state_channel_order = 1<<10
		};
		virtual void on_state_changed(state s) = 0;
	};

	struct visual_frontend_callback
	{
		virtual ~visual_frontend_callback() {}
		enum replaygain_value {
			replaygain_album_gain, replaygain_track_gain, replaygain_album_peak, replaygain_track_peak
		};
		virtual double get_track_length() const = 0;
		virtual double get_playback_position() const = 0;
		virtual bool is_cursor_visible() const = 0;
		virtual bool is_seeking() const = 0;
		virtual double get_seek_position() const = 0;
		virtual float get_replaygain(replaygain_value) const = 0;
		virtual bool get_playable_location(playable_location&) const = 0;
		virtual bool get_waveform(service_ptr_t<waveform>&) const = 0;
		virtual color get_color(config::color) const = 0;
		virtual CSize get_size() const = 0;
		virtual config::orientation get_orientation() const = 0;
		virtual bool get_shade_played() const = 0;
		virtual config::display_mode get_display_mode() const = 0;
		virtual bool get_downmix_display() const = 0;
		virtual void get_channel_infos(pfc::list_t<channel_info>&) const = 0;
	};

	struct visual_frontend_callback_setter {
		virtual ~visual_frontend_callback_setter() {}
		// Setters
		virtual void set_track_length(double v) = 0;
		virtual void set_playback_position(double v) = 0;
		virtual void set_cursor_visible(bool t) = 0;
		virtual void set_seeking(bool t) = 0;
		virtual void set_seek_position(double v) = 0;
		virtual void set_replaygain(visual_frontend_callback::replaygain_value e, float v) = 0;
		virtual void set_playable_location(playable_location const& loc) = 0;
		virtual void set_waveform(service_ptr_t<waveform> const& w) = 0;
		virtual void set_color(config::color e, color c) = 0;
		virtual void set_size(CSize size) = 0;
		virtual void set_orientation(config::orientation o) = 0;
		virtual void set_shade_played(bool b) = 0;
		virtual void set_display_mode(config::display_mode mode) = 0;
		virtual void set_downmix_display(bool downmix) = 0;
		virtual void set_channel_infos(pfc::list_t<channel_info> const&) = 0;
	};

	struct visual_frontend_factory
	{
		virtual ~visual_frontend_factory() {}
		virtual boost::shared_ptr<visual_frontend> create() const = 0;
	};

	template <typename FE>
	struct visual_frontend_traits
	{
		virtual boost::shared_ptr<FE> create() const { return boost::make_shared<FE>(); }
	};
}