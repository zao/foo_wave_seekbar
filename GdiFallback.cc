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

	void gdi_fallback_frontend::draw()
	{
		if (CPaintDC dc = wnd)
		{
			auto size = callback.get_size(), true_size = size;
			back_dc->BitBlt(0, 0, size.cx, size.cy, *wave_dc, 0, 0, SRCCOPY);

			bool vertical = callback.get_orientation() == config::orientation_vertical;
			if (vertical)
				std::swap(size.cx, size.cy);

			bool flip = callback.get_flip_display();

			auto draw_bar = [&](CPoint p1, CPoint p2)
			{
				if (flip)
				{
					p1.x = size.cx - p1.x;
					p2.x = size.cx - p2.x;
				}
				back_dc->SelectPen(*pen_selection);
				back_dc->MoveTo(orientate(p1));
				back_dc->LineTo(orientate(p2));
			};

			auto pos = callback.get_playback_position();
			auto len = callback.get_track_length();
			
			if (callback.get_shade_played())
			{
				color c = callback.get_color(config::color_highlight);
				c.a = 0.3f;
				CPoint p = orientate(CPoint((int)(pos * size.cx / len), size.cy));
				BLENDFUNCTION bf = { AC_SRC_OVER, 0, 0x40, 0 };
				if (p.x * p.y)
				{
					if (!flip)
						back_dc->AlphaBlend(0, 0, p.x, p.y, *shade_dc, 0, 0, 1, 1, bf);
					else
					{
						CPoint LR = orientate(CPoint(size.cx, size.cy));
						if (vertical)
							p = CPoint(0, LR.y - p.y);
						else
							p = CPoint(size.cx - p.x, 0);
						back_dc->AlphaBlend(p.x, p.y, LR.x, LR.y, *shade_dc, 0, 0, 1, 1, bf);
					}
				}
			}

			if (callback.is_cursor_visible())
			{
				draw_bar(
					CPoint((int)(pos * size.cx / len), 0),
					CPoint((int)(pos * size.cx / len), size.cy));

				if (callback.is_seeking())
				{
					auto seek_pos = callback.get_seek_position();
					draw_bar(
						CPoint((int)(seek_pos * size.cx / len), 0),
						CPoint((int)(seek_pos * size.cx / len), size.cy));
				}
			}
			dc.BitBlt(0, 0, true_size.cx, true_size.cy, *back_dc, 0, 0, SRCCOPY);
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
		
		if (!shade_dc)
		{
			CClientDC dc(wnd);
			shade_dc.reset(new mem_dc(dc, wave::size(1, 1)));
		}
		color c = callback.get_color(config::color_highlight);
		shade_dc->SetPixel(0, 0, color_to_xbgr(c));
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

		{
			util::ScopedEvent se("GDI Frontend", "DC reset");
			CClientDC win_dc(wnd);
			back_dc.reset(new mem_dc(win_dc, size));
			wave_dc.reset(new mem_dc(win_dc, size));
		}

		wave_dc->FillRect(CRect(0, 0, size.cx, size.cy), *brush_background);

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
				wave_dc->SelectPen(*pen_foreground);

				color bg = callback.get_color(config::color_background);
				color txt = callback.get_color(config::color_foreground);
				float4 backgroundColor(bg.r, bg.g, bg.b, bg.a);
				float4 textColor(txt.r, txt.g, txt.b, txt.a);
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
				#if 1
				size_t target_rows = vertical ? size.cx : size.cy;
				size_t target_cols = vertical ? size.cy : size.cx;
				std::vector<DWORD> line_storage(target_cols);
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
						color cc(c.x, c.y, c.z, c.w);

						line_storage[target_x] = color_to_xrgb(cc);
					}
					size_t channel_offset = (size_t)(outer_size.cy * squash);
					if (vertical) {
						wave_dc->SetDIBitsToDevice(channel_offset, target_y, line_storage.size(), 1, 0, 0, 0, 1,
							line_storage.data(), &bmi, DIB_RGB_COLORS);
					}
					else {
						wave_dc->SetDIBitsToDevice(0, target_y + channel_offset, line_storage.size(), 1, 0, 0, 0, 1,
							line_storage.data(), &bmi, DIB_RGB_COLORS);
					}
				}
				#else
				for (size_t x = 0; x < (size_t)size.cx; ++x)
				{	
					auto sample = samples[x];
					for (size_t y = 0; y < (size_t)size.cy; ++y)
					{
						size_t out_y = y + (size_t)(outer_size.cy * squash);
						float4 c;
						tc.y = 1.0f - 2.0f * (float)y / (float)size.cy;
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
						color cc(c.x, c.y, c.z, c.w);

						wave_dc->SetPixelV(vertical ? out_y : x, vertical ? x : out_y, color_to_xbgr(cc));
					}
				}
				#endif
				++quad_index;
			});
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