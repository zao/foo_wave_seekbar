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

	struct visual_frontend
	{
		virtual ~visual_frontend() {};
		virtual void clear() = 0;
		virtual void draw() = 0;
		virtual void present() = 0;

		enum state {
			state_color = 1,
			state_replaygain = 2,
			state_position = 4,
			state_size = 8,
			state_data = 16,
			state_track = 32,
			state_orientation = 64,
			state_shade_played = 128
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