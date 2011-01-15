#include "PchSeekbar.h"
#include "GdiFallback.h"
#include "Helpers.h"

namespace wave
{
	gdi_fallback_frontend::gdi_fallback_frontend(HWND wnd, CSize, visual_frontend_data& data)
		: wnd(wnd), data(data)
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
			CSize size = data.size.get(), true_size = size;
			back_dc->BitBlt(0, 0, size.cx, size.cy, *wave_dc, 0, 0, SRCCOPY);

			bool vertical = data.orientation.get() == config::orientation_vertical;
			if (vertical)
				std::swap(size.cx, size.cy);

			bool flip = data.flip_display.get();

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

			auto pos = data.playback_position.get();
			auto len = data.track_length.get();
			
			if (data.shade_played.get())
			{
				color c = data.highlight_color.get();
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

			if (data.cursor_visible.get())
			{
				draw_bar(
					CPoint((int)(pos * size.cx / len), 0),
					CPoint((int)(pos * size.cx / len), size.cy));

				if (data.seeking.get())
				{
					auto pos = data.seek_position.get();
					draw_bar(
						CPoint((int)(pos * size.cx / len), 0),
						CPoint((int)(pos * size.cx / len), size.cy));
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
#define PEN_FROM_COLOR(Name) { auto c = data.##Name##_color.get(); pen_##Name.reset(new CPen); pen_##Name->CreatePen(PS_SOLID, 0, color_to_xbgr(c)); }
#define SOLID_BRUSH_FROM_COLOR(Name) { auto c = data.##Name##_color.get(); brush_##Name.reset(new CBrush); brush_##Name->CreateSolidBrush(color_to_xbgr(c)); }
		PEN_FROM_COLOR(foreground);
		PEN_FROM_COLOR(highlight);
		PEN_FROM_COLOR(selection);
		SOLID_BRUSH_FROM_COLOR(background);
#undef PEN_FROM_COLOR
#undef SOLID_BRUSH_FROM_COLOR
		
		if (!shade_dc)
		{
			CClientDC dc(wnd);
			shade_dc.reset(new mem_dc(dc, CSize(1, 1)));
		}
		color c = data.highlight_color.get();
		shade_dc->SetPixel(0, 0, color_to_xbgr(c));
	}

	void gdi_fallback_frontend::release_objects()
	{
		pen_foreground.reset();
		pen_highlight.reset();
		pen_selection.reset();
		brush_background.reset();
	}

	D3DXVECTOR4* saturate(D3DXVECTOR4* in)
	{
		in->x = std::max(0.0f, std::min(1.0f, in->x));
		in->y = std::max(0.0f, std::min(1.0f, in->y));
		in->z = std::max(0.0f, std::min(1.0f, in->z));
		in->w = std::max(0.0f, std::min(1.0f, in->w));
		return in;
	}

	void gdi_fallback_frontend::update_data()
	{
		CSize size = data.size.get();

		{
			CClientDC win_dc(wnd);
			back_dc.reset(new mem_dc(win_dc, size));
			wave_dc.reset(new mem_dc(win_dc, size));
		}

		wave_dc->FillRect(CRect(0, 0, size.cx, size.cy), *brush_background);

		bool vertical = data.orientation.get() == config::orientation_vertical;
		if (vertical)
			std::swap(size.cx, size.cy);

		bool flip = data.flip_display.get();
		
		service_ptr_t<waveform> w;
		if (w = data.waveform.get(), w.is_valid())
		{
			if (data.downmix_display.get())
				w = downmix_waveform(w);

			pfc::list_t<channel_info> infos;
			data.get_channel_infos(infos);

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
				pfc::list_hybrid_t<float, 2048> avg_min, avg_max, avg_rms;
				w->get_field("minimum", index, avg_min);
				w->get_field("maximum", index, avg_max);
				w->get_field("rms", index, avg_rms);
				wave_dc->SelectPen(*pen_foreground);

				color bg = data.background_color.get();
				color txt = data.foreground_color.get();
				D3DXVECTOR4 backgroundColor(bg.r, bg.g, bg.b, bg.a);
				D3DXVECTOR4 textColor(txt.r, txt.g, txt.b, txt.a);
				D3DXVECTOR2 tc;

				float squash = (float)quad_index / (float)index_count;
				auto& outer_size = size;
				CSize size(outer_size.cx, outer_size.cy / index_count);

				float dx = 1.0f / (float)size.cx;
				float dy = 1.0f / (float)size.cy;
				for (size_t x = 0; x < (size_t)size.cx; ++x)
				{
					tc.x = (float)x / (float)size.cx;
					size_t ix = (x * 2048ul / size.cx);
					if (flip)
						ix = 2047ul - ix;

					D3DXVECTOR4 sample(avg_min[ix], avg_max[ix], avg_rms[ix], 1);
		#if 1
					for (size_t y = 0; y < (size_t)size.cy; ++y)
					{
						size_t out_y = y + (size_t)(outer_size.cy * squash);
						D3DXVECTOR4 c;
						tc.y = 1.0f - 2.0f * (float)y / (float)size.cy;
						float below = tc.y - sample.x;
						float above = tc.y - sample.y;
						float factor = std::min(fabs(below), fabs(above));
						bool outside = (below < 0 || above > 0);
						bool inside_rms = fabs(tc.y) <= sample.z;

						if (outside)
							c = backgroundColor;
						else
							D3DXVec4Lerp(&c, &backgroundColor, &textColor, 7.0f * factor);
						
						saturate(&c);
						color cc(c.x, c.y, c.z, c.w);

						wave_dc->SetPixelV(vertical ? out_y : x, vertical ? x : out_y, color_to_xbgr(cc));
					}
		#else
					size_t ix = std::min(2047ul, x * 2048ul / size.cx);
					CPoint p1(x, (int)(size.cy * (0.5 - avg_min[ix] * 0.5)));
					CPoint p2(x, (int)(size.cy * (0.5 - avg_max[ix] * 0.5)));
					wave_dc->MoveTo(orientate(p1));
					wave_dc->LineTo(orientate(p2));
		#endif
				}
				++quad_index;
			});
		}
	}

	CPoint gdi_fallback_frontend::orientate(CPoint p)
	{
		if (data.orientation.get() == config::orientation_vertical)
			return CPoint(p.y, p.x);
		return p;
	}
}