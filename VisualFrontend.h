#pragma once
#include "SeekbarState.h"
#include "ActiveData.h"

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
				
		enum replaygain_value
		{
			replaygain_album_gain,
			replaygain_track_gain,
			replaygain_album_peak,
			replaygain_track_peak
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

	struct visual_frontend : enable_shared_from_this<visual_frontend>
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
		virtual void show_configuration(CWindow parent) { }
		virtual void close_configuration() { }
	};

	struct color_set
	{
		active_data<color> background_color, foreground_color, highlight_color, selection_color; // brittle order, do not change

		active_data<color> get_color(config::color which) const {
			switch (which) {
			case config::color_background: return background_color;
			case config::color_foreground: return foreground_color;
			case config::color_highlight: return highlight_color;
			case config::color_selection: return selection_color;
			default: throw std::runtime_error("invalid color enumerand for get_color");
			}
		}

		active_data<color> operator [] (config::color which) const {
			return get_color(which);
		}
	};

	struct add_item_impl
	{
		template <typename C, typename T>
		struct result { typedef C& type; };

		template <typename C, typename T>
		C& operator () (C& c, T arg) const { c.add_item(arg); return c; }
	};
	
	namespace { boost::phoenix::function<add_item_impl> add_item; }

	struct display_data
	{
		virtual void get_channel_infos(pfc::list_t<channel_info>& out) const
		{
			using boost::phoenix::ref; using boost::phoenix::arg_names::arg1;
			pfc::list_t<channel_info> const& infos = channel_infos;
			out.remove_all();
			infos.enumerate(add_item(ref(out), arg1));
		}

		virtual void set_channel_infos(pfc::list_t<channel_info> const& in)
		{
			using boost::phoenix::ref; using boost::phoenix::arg_names::arg1;
			pfc::list_t<channel_info> infos = channel_infos;
			infos.remove_all();
			in.enumerate(add_item(ref(infos), arg1));
			channel_infos = infos;
		}

		active_data<CSize> size;
		active_data<config::orientation> orientation;
		active_data<bool> shade_played;
		active_data<config::display_mode> display_mode;
		active_data<bool> downmix_display, flip_display;
		active_data<pfc::list_t<channel_info>> channel_infos;

		display_data()
			: orientation(config::orientation_horizontal), shade_played(true), downmix_display(false), flip_display(false)
		{}
	};

	struct replaygain_data
	{
		replaygain_data()
			: rg_album_gain(0.0), rg_track_gain(0.0), rg_album_peak(0.0), rg_track_peak(0.0)
		{}

		active_data<float> rg_album_gain, rg_track_gain, rg_album_peak, rg_track_peak;

		// Getters
		active_data<float> get_replaygain(config::replaygain_value e) const {
			switch(e) {
			case config::replaygain_album_gain: return rg_album_gain;
			case config::replaygain_track_gain: return rg_track_gain;
			case config::replaygain_album_peak: return rg_album_peak;
			case config::replaygain_track_peak: return rg_track_peak;
			default: throw std::runtime_error("invalid enumerand for replaygain");
			}
		}
	};

	struct visual_frontend_data : color_set, display_data, replaygain_data
	{		
		visual_frontend_data()
			: track_length(1.0), playback_position(0.0), cursor_visible(false), seeking(false)
			, seek_position(0.0)
		{}

		active_data<bool> cursor_visible, seeking;
		active_data<double> playback_position, seek_position, track_length;

		active_data<playable_location_impl> location;
		active_data<service_ptr_t<wave::waveform>> waveform;
	};
	
	struct visual_frontend_config
	{
		virtual ~visual_frontend_config() {}
		
		virtual bool get_configuration_string(GUID key, pfc::string& out) const = 0;
		virtual void set_configuration_string(GUID key, pfc::string const& value) = 0;
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