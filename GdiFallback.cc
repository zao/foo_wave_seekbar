//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "GdiFallback.h"
#include "Helpers.h"
#include "frontend_sdk/FrontendHelpers.h"

namespace wave
{
	gdi_fallback_frontend::gdi_fallback_frontend(HWND wnd, wave::size, visual_frontend_callback& callback, visual_frontend_config& conf)
		: wnd(wnd), callback(callback)
	{
		create_objects();
		on_state_changed((state)~0);
	}

	gdi_fallback_frontend::~gdi_fallback_frontend()
	{
		release_objects();
	}

	void gdi_fallback_frontend::clear()
	{}

	inclusive_rect make_inclusive_rect(int left, int top, int right, int bottom)
	{
		inclusive_rect r = { left, top, right, bottom };
		return r;
	}

	inclusive_rect make_inclusive_rect(point p, size dimension)
	{
		return make_inclusive_rect(p.x, p.y, p.x+dimension.cx-1, p.y+dimension.cy-1);
	}

	point make_point(int x, int y)
	{
		point p = { x, y };
		return p;
	}

	inclusive_rect transpose(inclusive_rect rect)
	{
		std::swap(rect.left, rect.top);
		std::swap(rect.right, rect.bottom);
		return rect;
	}

	inclusive_rect right_align(inclusive_rect rect, int new_right)
	{
		int delta = new_right - rect.right;
		rect.right += delta;
		rect.left += delta;
		return rect;
	}

	inclusive_rect bottom_align(inclusive_rect rect, int new_bottom)
	{
		int delta = new_bottom - rect.bottom;
		rect.bottom += delta;
		rect.top += delta;
		return rect;
	}

	inclusive_rect rect_union(inclusive_rect a, inclusive_rect b)
	{
		a.left = std::min(a.left, b.left);
		a.top = std::min(a.top, b.top);
		a.right = std::max(a.right, b.right);
		a.bottom = std::max(a.bottom, b.bottom);
		return a;
	}

	inclusive_rect reorient_rect(inclusive_rect rect, int max_major_point, bool vertical, bool reverse_major_axis) {
		if (reverse_major_axis) {
			rect = right_align(rect, max_major_point);
		}
		if (vertical) {
			rect = transpose(rect);
		}
		return rect;
	}

	wave::size reorient_size(wave::size size, bool vertical) {
		if (vertical) {
			std::swap(size.cx, size.cy);
		}
		return size;
	}
	
	CPoint reorient_point(CPoint point, CSize canvas_size, bool vertical, bool reverse_major_axis) {
		if (reverse_major_axis) {
			point.x = canvas_size.cx - point.x;
		}
		if (vertical) {
			std::swap(point.x, point.y);
		}
		return point;
	}

	void unity_blit(HDC dst_dc, CRect rect, HDC src_dc)
	{
		CDCHandle h = dst_dc;
		h.BitBlt(rect.left, rect.top, rect.Width(), rect.Height(),
			src_dc, rect.left, rect.top, SRCCOPY);
	}

	void unity_blit(HDC dst_dc, inclusive_rect rect, HDC src_dc)
	{
		CRect r(rect.left, rect.top, rect.right+1, rect.bottom+1);
		unity_blit(dst_dc, r, src_dc);
	}

	std::pair<boost::optional<inclusive_rect>, boost::optional<inclusive_rect>> split_rect(inclusive_rect rect, unsigned where)
	{
		inclusive_rect other = rect;
		rect.right = where-1;
		other.left = where+1;
		boost::optional<inclusive_rect> first, second;
		if (rect.right >= rect.left)
			first = rect;
		if (other.right >= other.left)
			second = other;
		return std::make_pair(first, second);
	}

	struct cursor_info
	{
		unsigned seeking_offset, position_offset;
		bool has_seeking, has_position;
	};

	cursor_info make_cursor_info(visual_frontend_callback const& callback)
	{
		auto seek_pos = callback.get_seek_position();
		auto playback_pos = callback.get_playback_position();
		auto track_length = callback.get_track_length();
		auto size = callback.get_size();
		auto major_axis_length = callback.get_orientation() == config::orientation_vertical
			? size.cy
			: size.cx;
		cursor_info info = {
			(unsigned)(seek_pos * major_axis_length / track_length),
			(unsigned)(playback_pos * major_axis_length / track_length),
			callback.is_seeking(),
			callback.is_cursor_visible() };
		return info;
	}

	enum class Shade
	{
		SHADED, UNSHADED
	};

	size contract_size(size s)
	{
		return size(s.cx-1, s.cy-1);
	}

	typedef std::pair<inclusive_rect, Shade> augmented_rect;
	template <typename IndirectRect>
	static void append_if_valid(std::vector<augmented_rect>& v, IndirectRect p, Shade shade)
	{
		if (p) v.push_back(augmented_rect(*p, shade));
	}

	// returns rect+shadedness
	std::vector<std::pair<inclusive_rect, Shade>> compute_waveform_rects(wave::size canvas_size, unsigned const* position, unsigned const* seek_position)
	{
		std::vector<augmented_rect> ret;
		auto canvas_rect = make_inclusive_rect(make_point(0, 0), canvas_size);
		if (!position) {
			if (seek_position) {
				/* [u] S [u] */
				auto halves = split_rect(canvas_rect, *seek_position);
				append_if_valid(ret, halves.first, Shade::UNSHADED);
				append_if_valid(ret, halves.second, Shade::UNSHADED);
				return ret;
			}
			else {
				/* [u] */
				append_if_valid(ret, &canvas_rect, Shade::UNSHADED);
			}
		}
		else {
			auto const P = *position;
			auto outer_halves = split_rect(canvas_rect, P);
			if (seek_position) {
				auto const S = *seek_position;
				if (S < P) {
					/* [s] S [s] P [u] : S<P */
					append_if_valid(ret, outer_halves.second, Shade::UNSHADED);
					if (outer_halves.first) {
						auto inner_halves = split_rect(*outer_halves.first, S);
						append_if_valid(ret, inner_halves.first, Shade::SHADED);
						append_if_valid(ret, inner_halves.second, Shade::SHADED);
					}
				}
				else if (*seek_position > *position) {
					/* [s] P [u] S [u] : S>P */
					append_if_valid(ret, outer_halves.first, Shade::SHADED);
					if (outer_halves.second) {
						auto inner_halves = split_rect(*outer_halves.second, S);
						append_if_valid(ret, inner_halves.first, Shade::UNSHADED);
						append_if_valid(ret, inner_halves.second, Shade::UNSHADED);
					}
				}
				else {
					/* [s] S [u] : S=P */
					append_if_valid(ret, outer_halves.first, Shade::SHADED);
					append_if_valid(ret, outer_halves.second, Shade::UNSHADED);
				}
			}
			else {
				/* [s] P [u] */
				append_if_valid(ret, outer_halves.first, Shade::SHADED);
				append_if_valid(ret, outer_halves.second, Shade::UNSHADED);
			}
		}
		return ret;
	}

	void gdi_fallback_frontend::draw()
	{
		bool vertical = callback.get_orientation() == config::orientation_vertical;
		bool flip = callback.get_flip_display();
		auto draw_bar = [&](HDC dc, CPoint p1, CPoint p2)
		{
			CDCHandle h = dc;
			h.SelectPen(*pen_selection);
			h.MoveTo(p1);
			h.LineTo(p2);
		};
		if (CPaintDC dc = wnd)
		{
			auto cursors = make_cursor_info(callback);
			auto has_cursor = cursors.has_position;
			auto is_seeking = cursors.has_seeking;
			auto size = callback.get_size();
			auto canvas_size = size;
			auto object_size = reorient_size(canvas_size, vertical);
			auto far_corner = make_point(size.cx-1, size.cy-1);
			auto len = callback.get_track_length();
			auto position = cursors.position_offset;
			auto seek_position = cursors.seeking_offset;
			auto shade_enabled = callback.get_shade_played();

			auto rects = compute_waveform_rects(canvas_size,
				has_cursor ? &position : nullptr,
				is_seeking ? &seek_position : nullptr);

			for (auto ar : rects) {
				auto rect = reorient_rect(ar.first, far_corner.x, false, flip);
				auto is_shaded = shade_enabled && ar.second == Shade::SHADED;
				unity_blit(dc, rect, is_shaded ? *shaded_wave_dc : *wave_dc);
			}
			if (is_seeking) {
				auto from = reorient_point(CPoint(seek_position, 0), far_corner.x, vertical, flip);
				auto to = reorient_point(CPoint(seek_position, canvas_size.cy), far_corner.x, vertical, flip);
				draw_bar(dc, from, to);
			}
			if (has_cursor && (!is_seeking || position != seek_position)) {
				auto from = reorient_point(CPoint(position, 0), far_corner.x, vertical, flip);
				auto to = reorient_point(CPoint(position, canvas_size.cy), far_corner.x, vertical, flip);
				draw_bar(dc, from, to);
			}
		}
	}

	void gdi_fallback_frontend::present()
	{}

	void gdi_fallback_frontend::on_state_changed(state s)
	{
#if 0
		if (s & state_position)
			update_effect_cursor();
		if (s & state_replaygain)
			update_replaygain();
#endif
		if (s & state_shade_played) {
			wnd.Invalidate(false);
		}
		if (s & state_position) {
			update_positions();
		}
		if (s & state_color)
		{
			release_objects();
			create_objects();
		}
		if (s & (state_data | state_size | state_orientation | state_color | state_channel_order | state_downmix_display | state_flip_display))
			update_data();
	}

	void gdi_fallback_frontend::create_objects()
	{
		auto pen_from_color = [&](config::color color, scoped_ptr<CPen>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new CPen);
			out->CreatePen(PS_SOLID, 0, color_to_xbgr(c));
		};
		auto solid_brush_from_color = [&](config::color color, scoped_ptr<CBrush>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new CBrush);
			out->CreateSolidBrush(color_to_xbgr(c));
		};
		pen_from_color(config::color_foreground, pen_foreground);
		pen_from_color(config::color_highlight, pen_highlight);
		pen_from_color(config::color_selection, pen_selection);
		solid_brush_from_color(config::color_background, brush_background);
	}

	void gdi_fallback_frontend::release_objects()
	{
		pen_foreground.reset();
		pen_highlight.reset();
		pen_selection.reset();
		brush_background.reset();
	}

	struct float4
	{
		float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
		float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
		float x, y, z, w;
	};

	float4* saturate(float4* in)
	{
		in->x = std::max(0.0f, std::min(1.0f, in->x));
		in->y = std::max(0.0f, std::min(1.0f, in->y));
		in->z = std::max(0.0f, std::min(1.0f, in->z));
		in->w = std::max(0.0f, std::min(1.0f, in->w));
		return in;
	}

	float4* lerp(float4* out, float4 const* a, float4 const* b, float f)
	{
		float const omf = 1.0f - f;
		out->x = omf * a->x + f * b->x;
		out->y = omf * a->y + f * b->y;
		out->z = omf * a->z + f * b->z;
		out->w = omf * a->w + f * b->w;
		return out;
	}

	void gdi_fallback_frontend::update_data()
	{
		util::ScopedEvent se("GDI Frontend", "update_data");
		auto size = callback.get_size();
		last_play_rect = make_inclusive_rect(make_point(0, 0), size);
		last_seek_rect = make_inclusive_rect(make_point(0, 0), size);

		{
			util::ScopedEvent se("GDI Frontend", "DC reset");
			CClientDC win_dc(wnd);
			wave_dc.reset(new mem_dc(win_dc, size));
			shaded_wave_dc.reset(new mem_dc(win_dc, size));
		}

		bool vertical = callback.get_orientation() == config::orientation_vertical;
		if (vertical)
			std::swap(size.cx, size.cy);

		bool flip = callback.get_flip_display();

		ref_ptr<waveform> w;
		if (callback.get_waveform(w))
		{
			if (callback.get_downmix_display() != config::downmix_none) {
				util::ScopedEvent se("Mixing", "Downmix waveform");
				switch (callback.get_downmix_display())
				{
				case config::downmix_mono:   if (w->get_channel_count() > 1) w = downmix_waveform(w, 1); break;
				case config::downmix_stereo: if (w->get_channel_count() > 2) w = downmix_waveform(w, 2); break;
				}
			}

			pfc::list_t<channel_info> infos;
			callback.get_channel_infos(list_array_sink<channel_info>(infos));

			auto channel_numbers = expand_flags(w->get_channel_map());
			pfc::list_t<int> channel_indices;
			infos.enumerate([&channel_indices, channel_numbers](channel_info const& info)
			{
				if (info.enabled)
				{
					auto I = std::find(channel_numbers.begin(), channel_numbers.end(), info.channel);
					decltype(I) first = channel_numbers.begin();
					if (I != channel_numbers.end())
					{
						channel_indices.add_item(std::distance(first, I));
					}
				}
			});

			int quad_index = 0;
			auto index_count = channel_indices.get_count();
			channel_indices.enumerate([&, index_count](int index)
			{
				auto& outer_size = size;
				unsigned low_y = (unsigned)(quad_index * outer_size.cy / index_count);
				unsigned high_y = (unsigned)((quad_index+1) * outer_size.cy / index_count);
				CSize size(outer_size.cx, high_y - low_y);
				util::EventArgs ea;
				ea["channel"] = std::to_string(index);
				ea["dimensions"] = std::to_string(size.cx) + "x" + std::to_string(size.cy);
				util::ScopedEvent se("GDI Frontend", "Shade channel", &ea);
				pfc::list_t<float> avg_min, avg_max, avg_rms;
				w->get_field("minimum", index, list_array_sink<float>(avg_min));
				w->get_field("maximum", index, list_array_sink<float>(avg_max));
				w->get_field("rms", index, list_array_sink<float>(avg_rms));

				color bg = callback.get_color(config::color_background);
				color txt = callback.get_color(config::color_foreground);
				color hi = callback.get_color(config::color_highlight);
				float4 backgroundColor(bg.r, bg.g, bg.b, bg.a);
				float4 textColor(txt.r, txt.g, txt.b, txt.a);
				float4 hilightColor(hi.r, hi.g, hi.b, hi.a);
				float4 tc;

				util::ScopedEvent se2("GDI Frontend", "Shading loop");
				float dx = 1.0f / (float)size.cx;
				float dy = 1.0f / (float)size.cy;
				std::vector<float4> samples(size.cx);
				for (size_t x = 0; x < (size_t)size.cx; ++x)
				{
					tc.x = (float)x / (float)size.cx;
					size_t ix = (x * 2048ul / size.cx);
					if (flip)
						ix = 2047ul - ix;

					samples[x] = float4(avg_min[ix], avg_max[ix], avg_rms[ix], 1);
				}
				BITMAPINFO bmi = {};
				{
					auto& h = bmi.bmiHeader;
					h.biSize = sizeof(h);
					h.biWidth = size.cx;
					h.biHeight = 1;
					h.biPlanes = 1;
					h.biBitCount = 32;
					h.biCompression = BI_RGB;
				}
				size_t target_rows = vertical ? size.cx : size.cy;
				size_t target_cols = vertical ? size.cy : size.cx;
				std::vector<DWORD> unshaded_row(target_cols);
				std::vector<DWORD> shaded_row(target_cols);
				for (size_t target_y = 0; target_y < target_rows; ++target_y) {
					for (size_t target_x = 0; target_x < target_cols; ++target_x) {
						auto x = vertical ? target_y : target_x;
						auto y = vertical ? target_x : target_y;
						float4 c;
						tc.y = 1.0f - 2.0f * (float)y / (float)size.cy;
						auto sample = samples[x];
						float below = tc.y - sample.x;
						float above = tc.y - sample.y;
						float factor = std::min(fabs(below), fabs(above));
						bool outside = (below < 0 || above > 0);
						bool inside_rms = fabs(tc.y) <= sample.z;

						if (outside)
							c = backgroundColor;
						else
							lerp(&c, &backgroundColor, &textColor, 7.0f * factor);
						
						saturate(&c);
						float4 shaded;
						lerp(&shaded, &hilightColor, &c, 0.75f);
						color cc(c.x, c.y, c.z, c.w);
						color ac(shaded.x, shaded.y, shaded.z, 1.0f);

						unshaded_row[target_x] = color_to_xrgb(cc);
						shaded_row[target_x] = color_to_xrgb(ac);
					}
					size_t channel_offset = low_y;
					if (vertical) {
						wave_dc->SetDIBitsToDevice(channel_offset, target_y, unshaded_row.size(), 1, 0, 0, 0, 1,
							unshaded_row.data(), &bmi, DIB_RGB_COLORS);
						shaded_wave_dc->SetDIBitsToDevice(channel_offset, target_y, shaded_row.size(), 1, 0, 0, 0, 1,
							shaded_row.data(), &bmi, DIB_RGB_COLORS);
					}
					else {
						wave_dc->SetDIBitsToDevice(0, target_y + channel_offset, unshaded_row.size(), 1, 0, 0, 0, 1,
							unshaded_row.data(), &bmi, DIB_RGB_COLORS);
						shaded_wave_dc->SetDIBitsToDevice(0, target_y + channel_offset, shaded_row.size(), 1, 0, 0, 0, 1,
							shaded_row.data(), &bmi, DIB_RGB_COLORS);
					}
				}
				++quad_index;
			});
		}
		wnd.Invalidate(FALSE);
	}

	bool invalidate_rect(HWND wnd, inclusive_rect rect, bool erase = false)
	{
		CRect r(rect.left, rect.top, rect.right+1, rect.bottom+1);
		return InvalidateRect(wnd, &r, erase ? TRUE : FALSE) == TRUE;
	}

	void gdi_fallback_frontend::update_positions()
	{
		auto canvas_size = callback.get_size();
		auto far_corner = make_point(canvas_size.cx-1, canvas_size.cy-1);
		auto vertical = callback.get_orientation() == config::orientation_vertical;
		auto flip = callback.get_flip_display();
		auto shade_played = callback.get_shade_played();
		auto cursors = make_cursor_info(callback);
		boost::optional<inclusive_rect> play_rect;
		boost::optional<inclusive_rect> seek_rect;
		if (cursors.has_position) {
			auto r = make_inclusive_rect(cursors.position_offset, 0, cursors.position_offset, far_corner.y);
			play_rect = reorient_rect(r, far_corner.x, vertical, flip);
		}
		if (cursors.has_seeking) {
			auto r = make_inclusive_rect(cursors.seeking_offset, 0, cursors.seeking_offset, far_corner.y);
			seek_rect = reorient_rect(r, far_corner.x, vertical, flip);
		}
		if (last_play_rect != play_rect) {
			if (shade_played && last_play_rect) {
				auto extent = play_rect ? *play_rect
					: reorient_rect(make_inclusive_rect(0, 0, 0, far_corner.y), far_corner.x, vertical, flip);
				auto combined_play_rect = rect_union(extent, *last_play_rect);
				invalidate_rect(wnd, combined_play_rect);
			}
			else {
				if (play_rect)
					invalidate_rect(wnd, *play_rect);
				if (last_play_rect)
					invalidate_rect(wnd, *last_play_rect);
			}
		}
		if (seek_rect)
			invalidate_rect(wnd, *seek_rect);
		if (last_seek_rect)
			invalidate_rect(wnd, *last_seek_rect);
		last_play_rect = play_rect;
		last_seek_rect = seek_rect;
	}

	CPoint gdi_fallback_frontend::orientate(CPoint p)
	{
		if (callback.get_orientation() == config::orientation_vertical)
			return CPoint(p.y, p.x);
		return p;
	}
}

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT(g_gdi_entrypoint, wave::config::frontend_gdi, wave::gdi_fallback_frontend)