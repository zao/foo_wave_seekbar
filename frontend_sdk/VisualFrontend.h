//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "../waveform_sdk/Waveform.h"
#include "../waveform_sdk/RefPointer.h"
#include <map>
#include <boost/assign.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>

namespace wave
{
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

		enum downmix
		{
			downmix_none = 0,
			downmix_mono = 1,
			downmix_stereo
		};

		__declspec(selectany) bool frontend_has_configuration[] =
		{
			true, false, false, false
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
			__declspec(selectany) wchar_t const* downmix[] =
			{
				L"Keep as-is", L"Mix-down to mono", L"Mix-down to stereo"
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

	struct size {
		explicit size(int cx = 0, int cy = 0) : cx(cx), cy(cy) {}
		int cx, cy;
	};

	inline color xbgr_to_color(DWORD c, BYTE a = 0xFFU) {
		float r = GetRValue(c) / 255.f,
		      g = GetGValue(c) / 255.f,
		      b = GetBValue(c) / 255.f;
		return color(r, g, b, a / 255.f);
	}
	
	inline COLORREF color_to_xrgb(color c)
	{
		return RGB(
			(BYTE)(c.a * c.b * 255),
			(BYTE)(c.a * c.g * 255),
			(BYTE)(c.a * c.r * 255)
			);
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

	struct screenshot_settings
	{
		void* context;
		void (*write_screenshot)(void* context, BYTE const* rgba);
		int32_t width, height;
		uint32_t flags;
	};

	struct visual_frontend : ref_base
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
			state_flip_display = 1<<10,
			state_channel_order = 1<<11
		};
		virtual void on_state_changed(state s) = 0;
		virtual void show_configuration(HWND parent) { }
		virtual void close_configuration() { }
		virtual int get_present_interval() const { return 100; } // milliseconds
		virtual void make_screenshot(screenshot_settings const* settings) {}
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
		virtual bool get_waveform(ref_ptr<waveform>&) const = 0;
		virtual color get_color(config::color) const = 0;
		virtual size get_size() const = 0;
		virtual config::orientation get_orientation() const = 0;
		virtual bool get_shade_played() const = 0;
		virtual config::display_mode get_display_mode() const = 0;
		virtual config::downmix get_downmix_display() const = 0;
		virtual bool get_flip_display() const = 0;
		virtual void get_channel_infos(array_sink<channel_info> const&) const = 0;

		virtual void run_in_main_thread(boost::function<void ()>) const = 0;
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
		virtual void set_waveform(ref_ptr<waveform> const& w) = 0;
		virtual void set_color(config::color e, color c) = 0;
		virtual void set_size(size size) = 0;
		virtual void set_orientation(config::orientation o) = 0;
		virtual void set_shade_played(bool b) = 0;
		virtual void set_display_mode(config::display_mode mode) = 0;
		virtual void set_downmix_display(config::downmix downmix) = 0;
		virtual void set_flip_display(bool flip) = 0;
		virtual void set_channel_infos(channel_info const*, size_t count) = 0;
	};

	struct text_sink
	{
		virtual void set(char const* text) const = 0;
	};

	struct std_string_sink : text_sink
	{
		std::string& s;
		explicit std_string_sink(std::string& s) : s(s) {}
		virtual void set(char const* text) const { s.assign(text); }
	};

	struct visual_frontend_config
	{
		virtual ~visual_frontend_config() {}
		
		virtual bool get_configuration_string(GUID key, text_sink const& out) const = 0;
		virtual void set_configuration_string(GUID key, char const* value) = 0;
	};
}

struct frontend_entrypoint
{
	virtual unsigned id() = 0;
	virtual ref_ptr<wave::visual_frontend> create(HWND, wave::size, wave::visual_frontend_callback&, wave::visual_frontend_config&) = 0;
};

extern "C"
{
	typedef frontend_entrypoint* (_cdecl *frontend_entrypoint_t)();
}

#define FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT_HOOK(Name, Id, Class, Hook) \
	static struct entrypoint_impl : frontend_entrypoint { \
		virtual unsigned id() { return Id; } \
		virtual ref_ptr<wave::visual_frontend> create(HWND wnd, wave::size size, wave::visual_frontend_callback& callback, wave::visual_frontend_config& config) override { \
			return ref_ptr<wave::visual_frontend>(new Class(wnd, size, callback, config)); \
		} \
	} g_frontend_entrypoint_impl; \
	extern "C" __declspec(dllexport) frontend_entrypoint* _cdecl Name() { { Hook(); } return &g_frontend_entrypoint_impl; }
//


#define FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT(Name, Id, Class) \
        FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT_HOOK(Name, Id, Class, []{})
//

#define FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT_HOOK(Id, Class, Hook) \
        FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT_HOOK(g_seekbar_frontend_entrypoint, Id, Class, Hook)
//

#define FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT(Id, Class) \
        FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT_HOOK(Id, Class, []{})
//
