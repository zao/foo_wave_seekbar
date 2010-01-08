#include "PchSeekbar.h"
#include "GdiFallback.h"

namespace wave
{
	gdi_fallback_frontend::gdi_fallback_frontend(HWND wnd, CSize, visual_frontend_callback& callback)
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
			CSize size = callback.get_size(), true_size = size;
			back_dc->BitBlt(0, 0, size.cx, size.cy, *wave_dc, 0, 0, SRCCOPY);

			if (callback.get_orientation() == config::orientation_vertical)
				std::swap(size.cx, size.cy);

			auto draw_bar = [&](CPoint p1, CPoint p2)
			{
				back_dc->SelectPen(*pen_selection);
				back_dc->MoveTo(p1);
				back_dc->LineTo(p2);
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
					back_dc->AlphaBlend(0, 0, p.x, p.y, *shade_dc, 0, 0, 1, 1, bf);
			}

			if (callback.is_cursor_visible())
			{
				draw_bar(
					CPoint((int)(pos * size.cx / len), 0),
					CPoint((int)(pos * size.cx / len), size.cy));

				if (callback.is_seeking())
				{
					auto pos = callback.get_seek_position();
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
		if (s & (state_data | state_size | state_orientation | state_color))
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
			shade_dc.reset(new mem_dc(dc, CSize(1, 1)));
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

	void gdi_fallback_frontend::update_data()
	{
		CSize size = callback.get_size();

		{
			CClientDC win_dc(wnd);
			back_dc.reset(new mem_dc(win_dc, size));
			wave_dc.reset(new mem_dc(win_dc, size));
		}

		wave_dc->FillRect(CRect(0, 0, size.cx, size.cy), *brush_background);

		if (callback.get_orientation() == config::orientation_vertical)
			std::swap(size.cx, size.cy);

		service_ptr_t<waveform> w;
		if (callback.get_waveform(w))
		{
			pfc::list_hybrid_t<float, 2048> avg_min, avg_max, avg_rms;
			w->get_field("minimum", avg_min);
			w->get_field("maximum", avg_max);
			w->get_field("rms", avg_rms);
			wave_dc->SelectPen(*pen_foreground);
			for (size_t x = 0; x < (size_t)size.cx; ++x)
			{
				size_t ix = std::min(2047ul, x * 2048ul / size.cx);
				CPoint p1(x, (int)(size.cy * (0.5 - avg_min[ix] * 0.5)));
				CPoint p2(x, (int)(size.cy * (0.5 - avg_max[ix] * 0.5)));
				wave_dc->MoveTo(orientate(p1));
				wave_dc->LineTo(orientate(p2));
			}
		}
	}

	CPoint gdi_fallback_frontend::orientate(CPoint p)
	{
		if (callback.get_orientation() == config::orientation_vertical)
			return CPoint(p.y, p.x);
		return p;
	}
}