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

	CRect reorient_rect(CRect rect, CSize canvas_size, bool vertical, bool reverse_major_axis) {
		if (reverse_major_axis) {
			rect.left = canvas_size.cx - rect.left;
			rect.right = canvas_size.cx - rect.right;
			CRect::SwapLeftRight(&rect);
		}
		if (vertical) {
			std::swap(rect.left, rect.top);
			std::swap(rect.right, rect.bottom);
		}
		return rect;
	}

	CSize reorient_size(CSize size, CSize canvas_size, bool vertical, bool reverse_major_axis) {
		if (reverse_major_axis) {
			size.cx = canvas_size.cx - size.cx;
		}
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

	std::pair<CRect, CRect> split_rect(CRect rect, unsigned where)
	{
		CRect other = rect;
		other.left = where+1;
		rect.right = where;
		return std::make_pair(rect, other);
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
		cursor_info info = {
			(unsigned)(seek_pos * size.cx / track_length),
			(unsigned)(playback_pos * size.cx / track_length),
			callback.is_seeking(),
			callback.is_cursor_visible() };
		return info;
	}

	// returns rect+shadedness
	std::vector<std::pair<CRect, bool>> compute_waveform_rects(CSize canvas_size, unsigned const* position, unsigned const* seek_position)
	{
		bool const SHADED = true;
		bool const UNSHADED = false;
		typedef std::pair<CRect, bool> augmented_rect;
		CRect canvas_rect(CPoint(0, 0), canvas_size);
		if (!position) {
			if (seek_position) {
				/* [u) S [u) */
				auto halves = split_rect(canvas_rect, *seek_position);
				std::array<augmented_rect, 2> rects = {
					augmented_rect(halves.first, UNSHADED),
					augmented_rect(halves.second, UNSHADED) };
				return std::vector<augmented_rect>(rects.begin(), rects.end());
			}
			/* [u) */
			return std::vector<augmented_rect>(1, augmented_rect(canvas_rect, UNSHADED));
		}
		if (seek_position) {
			auto const S = *seek_position;
			auto const P = *position;
			auto outer_halves = split_rect(canvas_rect, P);
			if (S < P) {
				/* [s) S [s) P [u) : S<P */
				auto inner_halves = split_rect(outer_halves.first, S);
				std::array<augmented_rect, 3> rects = {
					augmented_rect(inner_halves.first, SHADED),
					augmented_rect(inner_halves.second, SHADED),
					augmented_rect(outer_halves.second, UNSHADED) };
				return std::vector<augmented_rect>(rects.begin(), rects.end());
			}
			else if (*seek_position > *position) {
				/* [s) P [u) S [u) : S>P */
				auto inner_halves = split_rect(outer_halves.second, S);
				std::array<augmented_rect, 3> rects = {
					augmented_rect(outer_halves.first, SHADED),
					augmented_rect(inner_halves.first, UNSHADED),
					augmented_rect(inner_halves.second, UNSHADED) };
				return std::vector<augmented_rect>(rects.begin(), rects.end());
			}
			/* [s) S [u) : S=P */
			std::array<augmented_rect, 2> rects = {
				augmented_rect(outer_halves.first, SHADED),
				augmented_rect(outer_halves.second, UNSHADED) };
			return std::vector<augmented_rect>(rects.begin(), rects.end());
		}
		/* [s) P [u) */
		auto halves = split_rect(canvas_rect, *position);
		std::array<augmented_rect, 2> rects = {
			augmented_rect(halves.first, SHADED),
			augmented_rect(halves.second, UNSHADED) };
		return std::vector<augmented_rect>(rects.begin(), rects.end());
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
			auto size = callback.get_size(), true_size = size;
			auto canvas_size = CSize(size.cx, size.cy);
			auto len = callback.get_track_length();
			auto position = cursors.position_offset;
			auto seek_position = cursors.seeking_offset;

			auto rects = compute_waveform_rects(canvas_size, has_cursor ? &position : nullptr,
				is_seeking ? &seek_position : nullptr);

			for (auto ar : rects) {
				auto rect = reorient_rect(ar.first, canvas_size, vertical, flip);
				auto is_shaded = ar.second;
				if (!rect.IsRectEmpty()) {
					unity_blit(dc, rect, is_shaded ? *shaded_wave_dc : *wave_dc);
				}
			}
			if (is_seeking) {
				auto from = reorient_point(CPoint(seek_position, 0), canvas_size, vertical, flip);
				auto to = reorient_point(CPoint(seek_position, size.cx), canvas_size, vertical, flip);
				draw_bar(dc, from, to);
			}
			if (has_cursor && (!is_seeking || position != seek_position)) {
				auto from = reorient_point(CPoint(position, 0), canvas_size, vertical, flip);
				auto to = reorient_point(CPoint(position, size.cx), canvas_size, vertical, flip);
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
		if (s & state_position) {
			update_positions();
		}
		if (s & state_shade_played) {
			// invalidate shaded side
		}
		if (s & state_color)
		{
			release_objects();
			create_objects();
		}
		if (s & (state_data | state_size | state_orientation | state_color | state_channel_order | state_downmix_display | state_flip_display))
			update_data();
#if 0
		if (s & state_shade_played)
			update_shade_played();
#endif
	}

	void gdi_fallback_frontend::create_objects()
	{
		auto pen_from_color = [&](config::color color, std::unique_ptr<CPen>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new CPen);
			out->CreatePen(PS_SOLID, 0, color_to_xbgr(c));
		};
		auto solid_brush_from_color = [&](config::color color, std::unique_ptr<CBrush>& out)
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
		last_play_rect = CRect(0, 0, size.cx, size.cy);
		last_seek_rect = CRect(0, 0, size.cx, size.cy);

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
				CSize size(outer_size.cx, outer_size.cy / index_count);
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

				float squash = (float)quad_index / (float)index_count;

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
					size_t channel_offset = (size_t)(outer_size.cy * squash);
					if (vertical) {
						wave_dc->SetDIBitsToDevice(channel_offset, target_y, unshaded_row.size(), 1, 0, 0, 0, 1,
							unshaded_row.data(), &bmi, DIB_RGB_COLORS);
						shaded_wave_dc->SetDIBitsToDevice(channel_offset, target_y, shaded_row.size(), 1, 0, 0, 0, 1,
							shaded_row.data(), &bmi, DIB_RGB_COLORS);
					}
					else {
						wave_dc->SetDIBitsToDevice(0, target_y + channel_offset, unshaded_row.size(), 1, 0, 0, 0, 1,
							unshaded_row.data(), &bmi, DIB_RGB_COLORS);
						shaded_wave_dc->SetDIBitsToDevice(channel_offset, target_y, shaded_row.size(), 1, 0, 0, 0, 1,
							shaded_row.data(), &bmi, DIB_RGB_COLORS);
					}
				}
				++quad_index;
			});
		}
		wnd.Invalidate(FALSE);
	}

	void gdi_fallback_frontend::update_positions()
	{
		auto size = callback.get_size();
		CSize canvas_size(size.cx, size.cy);
		auto vertical = callback.get_orientation() == config::orientation_vertical;
		auto flip = callback.get_flip_display();
		auto shade_played = callback.get_shade_played();
		auto cursors = make_cursor_info(callback);
		wave::optional<CRect> play_rect;
		wave::optional<CRect> seek_rect;
		if (cursors.has_position) {
			CRect r(cursors.position_offset, 0, cursors.position_offset+1, size.cy);
			play_rect = reorient_rect(r, canvas_size, vertical, flip);
		}
		if (cursors.has_seeking) {
			CRect r(cursors.seeking_offset, 0, cursors.seeking_offset+1, size.cy);
			seek_rect = reorient_rect(r, canvas_size, vertical, flip);
		}
		if (last_play_rect != play_rect) {
			if (shade_played && last_play_rect) {
				CRect extent = play_rect ? *play_rect
					: reorient_rect(CRect(0, 0, 1, size.cy), canvas_size, vertical, flip);
				CRect combined_play_rect;
				combined_play_rect.UnionRect(extent, &*last_play_rect);
				InvalidateRect(wnd, &combined_play_rect, FALSE);
			}
			else {
				if (play_rect)
					InvalidateRect(wnd, &*play_rect, FALSE);
				if (last_play_rect)
					InvalidateRect(wnd, &*last_play_rect, FALSE);
			}
		}
		if (seek_rect)
			InvalidateRect(wnd, &*seek_rect, FALSE);
		if (last_seek_rect)
			InvalidateRect(wnd, &*last_seek_rect, FALSE);
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