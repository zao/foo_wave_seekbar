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
		: wnd(wnd), callback(callback), cached_rects_valid(false)
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

	int map_normalized_float_to_unsigned_range(unsigned dst_low, unsigned dst_high, float value)
	{
		unsigned dst_delta = dst_high - dst_low;
		return dst_low + std::min(dst_delta-1, (unsigned)(dst_delta * value));
	}

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
		float seeking_fraction, position_fraction;
		bool has_seeking, has_position;
	};

	cursor_info make_cursor_info(visual_frontend_callback const& callback)
	{
		auto seek_pos = callback.get_seek_position();
		auto playback_pos = callback.get_playback_position();
		auto track_length = callback.get_track_length();
		cursor_info info = {
			(float)(seek_pos / track_length),
			(float)(playback_pos / track_length),
			callback.is_seeking(),
			callback.is_cursor_visible() };
		return info;
	}

	std::vector<std::tuple<unsigned, unsigned, bool>> compute_waveform_regions(size_t major_extent, float const* position, float const* seek)
	{
		if (major_extent < 1) return {};
		enum class PointKind { ENDPOINT, SEEK, POSITION };
		std::map<unsigned, PointKind> assembly{{major_extent, PointKind::ENDPOINT}};
		if (seek)
			assembly[map_normalized_float_to_unsigned_range(0, major_extent, *seek)] = PointKind::SEEK;
		if (position)
			assembly[map_normalized_float_to_unsigned_range(0, major_extent, *position)] = PointKind::POSITION;
		std::vector<std::pair<unsigned, PointKind>> points{std::begin(assembly), std::end(assembly)};
		std::vector<std::tuple<unsigned, unsigned, bool>> regions;
		unsigned low_bound = 0u;
		bool shaded = true;
		for (auto& p : points) {
			unsigned pos = p.first;
			PointKind kind = p.second;
			if (pos > low_bound+1)
				regions.emplace_back(low_bound, pos-1, shaded);
			if (kind == PointKind::POSITION)
				shaded = false;
			low_bound = pos+1;
		}
		return regions;
	}

	CPoint derive_point(float x, float y, size_t screen_w, size_t screen_h, bool vertical, bool flip)
	{
		if (flip) x = 1.0f - x;
		if (vertical) {
			return CPoint(
				std::min(screen_w-1, (size_t)(screen_w * y)),
				std::min(screen_h-1, (size_t)(screen_h * x)));
		}
		else {
			return CPoint(
				std::min(screen_w-1, (size_t)(screen_w * x)),
				std::min(screen_h-1, (size_t)(screen_h * y)));
		}
	}

	void gdi_fallback_frontend::draw()
	{
		bool vertical = callback.get_orientation() == config::orientation_vertical;
		bool flip = callback.get_flip_display();
		auto draw_bar = [&](HDC dc, CPoint from, CPoint to)
		{
			if (from.x == to.x)
				++to.y;
			else
				++to.x;
			CDCHandle h = dc;
			h.SelectPen(*pen_selection);
			h.MoveTo(from);
			h.LineTo(to);
		};
		if (CPaintDC dc = wnd)
		{
			auto cursors = make_cursor_info(callback);
			auto has_cursor = cursors.has_position;
			auto is_seeking = cursors.has_seeking;
			auto size = callback.get_size(), true_size = size;
			auto canvas_size = CSize(size.cx, size.cy);
			auto len = callback.get_track_length();
			auto position = cursors.position_fraction;
			auto seek_position = cursors.seeking_fraction;
			auto should_shade = callback.get_shade_played();

			if (!has_cursor && !is_seeking) {
				unity_blit(dc, CRect({0, 0}, canvas_size), *wave_dc);
			}
			else {
				auto major_extent = vertical ? size.cy : size.cx;
				auto minor_extent = vertical ? size.cx : size.cy;
				auto regions = compute_waveform_regions(major_extent, has_cursor ? &position : nullptr, is_seeking ? &seek_position : nullptr);
				for (auto region : regions) {
					unsigned inc_low, inc_high;
					bool shaded;
					std::tie(inc_low, inc_high, shaded) = region;
					if (flip) {
						std::swap(inc_low, inc_high);
						inc_low = major_extent - inc_low - 1;
						inc_high = major_extent - inc_high - 1;
					}
					CPoint from(inc_low, 0), to(inc_high+1, minor_extent);
					if (vertical) {
						std::swap(from.x, from.y);
						std::swap(to.x, to.y);
					}
					unity_blit(dc, CRect(from, to), (shaded && should_shade) ? *shaded_wave_dc : *wave_dc);
				}
			}

			if (is_seeking) {
				auto from = derive_point(seek_position, 0.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
				auto to = derive_point(seek_position, 1.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
				draw_bar(dc, from, to);
			}
			if (has_cursor && (!is_seeking || position != seek_position)) {
				auto from = derive_point(position, 0.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
				auto to = derive_point(position, 1.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
				draw_bar(dc, from, to);
			}
		}
	}

	void gdi_fallback_frontend::present()
	{}

	void gdi_fallback_frontend::on_state_changed(state s)
	{
		if (s & state_shade_played) {
			// invalidate shaded side
			cached_rects_valid = false;
		}
		if (s & state_color) {
			cached_rects_valid = false;
			release_objects();
			create_objects();
		}
		if (s & (state_data | state_size | state_orientation | state_color | state_channel_order | state_downmix_display | state_flip_display)) {
			cached_rects_valid = false;
			update_data();
		}
		if (s & state_position) {
			update_positions();
		}
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
		auto bitmap_size = callback.get_size();
		{
			CClientDC win_dc(wnd);
			wave_dc.reset(new mem_dc(win_dc, bitmap_size));
			shaded_wave_dc.reset(new mem_dc(win_dc, bitmap_size));
		}

		bool vertical = callback.get_orientation() == config::orientation_vertical;
		bool flip = callback.get_flip_display();

		ref_ptr<waveform> w;
		if (!callback.get_waveform(w)) {
			color bg = callback.get_color(config::color_background);
			CRect all{0, 0, bitmap_size.cx, bitmap_size.cy};
			wave_dc->FillSolidRect(all, color_to_xbgr(bg));
			shaded_wave_dc->FillSolidRect(all, color_to_xbgr(bg));
		}
		else {
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
				size_t channel_width = bitmap_size.cx, channel_x_offset = 0;
				size_t channel_height = bitmap_size.cy, channel_y_offset = 0;
				if (vertical) {
					channel_x_offset = channel_width * quad_index / index_count;
					channel_width = channel_width * (quad_index+1) / index_count - channel_x_offset;
				}
				else {
					channel_y_offset = channel_height * quad_index / index_count;
					channel_height = channel_height * (quad_index+1) / index_count - channel_y_offset;
				}
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

				size_t major_extent = (size_t)(vertical ? bitmap_size.cy : bitmap_size.cx);
				std::vector<float4> samples(major_extent);
				for (size_t x = 0; x < major_extent; ++x) {
					size_t ix = (x * 2048ul / major_extent);
					samples[x] = float4(avg_min[ix], avg_max[ix], avg_rms[ix], 1);
				}
				BITMAPINFO bmi = {};
				{
					auto& h = bmi.bmiHeader;
					h.biSize = sizeof(h);
					h.biWidth = channel_width;
					h.biHeight = 1;
					h.biPlanes = 1;
					h.biBitCount = 32;
					h.biCompression = BI_RGB;
				}
				std::vector<DWORD> unshaded_row(channel_width);
				std::vector<DWORD> shaded_row(channel_width);
				for (size_t target_y = 0; target_y < channel_height; ++target_y) {
					for (size_t target_x = 0; target_x < channel_width; ++target_x) {
						size_t tc_x;
						float tc_y;
						if (vertical) {
							tc_x = flip ? (channel_height - target_y - 1) : target_y;
							tc_y = 1.0f - 2.0f * target_x / (float)(channel_width-1);
						}
						else {
							tc_x = flip ? (channel_width - target_x - 1) : target_x;
							tc_y = 1.0f - 2.0f * target_y / (float)(channel_height-1);
						}
						float4 c;
						auto sample = samples[tc_x];
						float below = tc_y - sample.x;
						float above = tc_y - sample.y;
						float factor = std::min(fabs(below), fabs(above));
						bool outside = (below < 0 || above > 0);
						bool inside_rms = fabs(tc_y) <= sample.z;

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
					wave_dc->SetDIBitsToDevice(channel_x_offset, target_y + channel_y_offset, unshaded_row.size(), 1, 0, 0, 0, 1,
						unshaded_row.data(), &bmi, DIB_RGB_COLORS);
					shaded_wave_dc->SetDIBitsToDevice(channel_x_offset, target_y + channel_y_offset, shaded_row.size(), 1, 0, 0, 0, 1,
						shaded_row.data(), &bmi, DIB_RGB_COLORS);
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
			auto from = derive_point(cursors.position_fraction, 0.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
			auto to = derive_point(cursors.position_fraction, 1.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
			play_rect = CRect(from, to);
			(*play_rect).InflateRect(0, 0, 1, 1);
		}
		if (cursors.has_seeking) {
			auto from = derive_point(cursors.seeking_fraction, 0.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
			auto to = derive_point(cursors.seeking_fraction, 1.0f, canvas_size.cx, canvas_size.cy, vertical, flip);
			seek_rect = CRect(from, to);
			(*seek_rect).InflateRect(0, 0, 1, 1);
			//CRect r(cursors.seeking_offset, 0, cursors.seeking_offset+1, size.cy);
			//seek_rect = reorient_rect(r, canvas_size, vertical, flip);
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

		if (!cached_rects_valid) {
			last_seek_rect = seek_rect;
			last_play_rect = play_rect;
			wnd.Invalidate(FALSE);
			cached_rects_valid = true;
		}
	}

	CPoint gdi_fallback_frontend::orientate(CPoint p)
	{
		if (callback.get_orientation() == config::orientation_vertical)
			return CPoint(p.y, p.x);
		return p;
	}
}

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT(g_gdi_entrypoint, wave::config::frontend_gdi, wave::gdi_fallback_frontend)