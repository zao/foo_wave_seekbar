#pragma once

namespace wave
{
	struct frontend_callback_impl : visual_frontend_callback, visual_frontend_callback_setter
	{
		// Getters
		virtual double get_track_length() const { return max(1.0, track_length); }
		virtual double get_playback_position() const { return playback_position; }
		virtual bool is_cursor_visible() const { return cursor_visible; }
		virtual bool is_seeking() const { return seeking; }
		virtual double get_seek_position() const { return seek_position; }
		virtual float get_replaygain(replaygain_value e) const {
			switch(e) {
			case replaygain_album_gain: return rg_album_gain;
			case replaygain_track_gain: return rg_track_gain;
			case replaygain_album_peak: return rg_album_peak;
			case replaygain_track_peak: return rg_track_peak;
			default: return 0.f;
			}
		}
		virtual bool get_playable_location(playable_location& loc) const {
			loc.set_path(location.get_path());
			loc.set_subsong(location.get_subsong());
			return true;
		}
		virtual bool get_waveform(service_ptr_t<waveform>& w) const { if (wf.is_valid()) w = wf; return wf.is_valid(); }
		virtual color get_color(config::color e) const {
			switch(e) {
			case config::color_background: return background_color;
			case config::color_foreground: return text_color;
			case config::color_highlight: return highlight_color;
			case config::color_selection: return selection_color;
			default: return color(1.0f, 105/255.f, 180/255.f);
			}
		}
		virtual CSize get_size() const { return size; }
		virtual config::orientation get_orientation() const { return orientation; }
		virtual bool get_shade_played() const { return shade_played; }
		virtual config::display_mode get_display_mode() const { return display_mode; }
		virtual bool get_downmix_display() const { return downmix_display; }
		virtual bool get_flip_display() const { return flip_display; }
		virtual void get_channel_infos(pfc::list_t<channel_info>& out) const { out = channel_infos; }

		// Setters
		virtual void set_track_length(double v) { track_length = v; }
		virtual void set_playback_position(double v) { playback_position = v; }
		virtual void set_cursor_visible(bool t) { cursor_visible = t; }
		virtual void set_seeking(bool t) { seeking = t; }
		virtual void set_seek_position(double v) { seek_position = v; }
		virtual void set_replaygain(replaygain_value e, float v) {
			switch (e) {
			case replaygain_album_gain: rg_album_gain = v; break;
			case replaygain_track_gain: rg_track_gain = v; break;
			case replaygain_album_peak: rg_album_peak = v; break;
			case replaygain_track_peak: rg_track_peak = v; break;
			}
		}
		virtual void set_playable_location(playable_location const& loc) {
			location.set_path(loc.get_path());
			location.set_subsong(loc.get_subsong());
		}
		virtual void set_waveform(service_ptr_t<waveform> const& w) { wf = w; }
		virtual void set_color(config::color e, color c) {
			switch(e) {
			case config::color_background: background_color = c; break;
			case config::color_foreground: text_color = c; break;
			case config::color_highlight: highlight_color = c; break;
			case config::color_selection: selection_color = c; break;
			}
		}
		virtual void set_size(CSize s) { size = s; }
		virtual void set_orientation(config::orientation o) { orientation = o; }
		virtual void set_shade_played(bool b) { shade_played = b; }
		virtual void set_display_mode(config::display_mode mode) { display_mode = mode; }
		virtual void set_downmix_display(bool b) { downmix_display = b; }
		virtual void set_flip_display(bool b) { flip_display = b; }
		virtual void set_channel_infos(pfc::list_t<channel_info> const& in) { channel_infos = in; }

		double track_length;
		double playback_position;
		bool cursor_visible;
		bool seeking;
		double seek_position;
		float rg_album_gain, rg_track_gain, rg_album_peak, rg_track_peak;
		playable_location_impl location;
		service_ptr_t<waveform> wf;
		color background_color, highlight_color, selection_color, text_color;
		CSize size;
		config::orientation orientation;
		bool shade_played;
		config::display_mode display_mode;
		bool downmix_display, flip_display;
		pfc::list_t<channel_info> channel_infos;

		frontend_callback_impl()
			: track_length(1.0), playback_position(0.0), cursor_visible(false), seeking(false)
			, seek_position(0.0), rg_album_gain(0.0), rg_track_gain(0.0), rg_album_peak(0.0), rg_track_peak(0.0)
			, orientation(config::orientation_horizontal), shade_played(true), downmix_display(false), flip_display(false)
		{}
	};
}