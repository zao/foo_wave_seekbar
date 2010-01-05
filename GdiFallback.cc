#include "PchSeekbar.h"
#include "GdiFallback.h"

namespace wave
{
	gdi_fallback_frontend::gdi_fallback_frontend(HWND wnd, CSize size, visual_frontend_callback& callback)
		: wnd(wnd), size(size), callback(callback)
	{
		Gdiplus::GdiplusStartupInput gpsi;
		Gdiplus::GdiplusStartup(&gdiplus_token, &gpsi, 0);
		create_objects();
		on_state_changed((state)~0);
	}

	gdi_fallback_frontend::~gdi_fallback_frontend()
	{
		release_objects();
		Gdiplus::GdiplusShutdown(gdiplus_token);
	}

	void gdi_fallback_frontend::clear()
	{}

	void gdi_fallback_frontend::draw()
	{
		PAINTSTRUCT ps = {};
		if (CDCHandle dc = wnd.BeginPaint(&ps))
		{
			//Gdiplus::Bitmap bmp(callback.get_size().cx, callback.get_size().cy);
			//scoped_ptr<Gdiplus::Graphics> g(Gdiplus::Graphics::FromImage(&bmp));
			Gdiplus::Graphics gdc(dc);
			Gdiplus::Graphics* g = &gdc;

			if (cached_bitmap)
			{
				g->DrawCachedBitmap(cached_bitmap.get(), 0, 0);
			}
			if (callback.is_cursor_visible())
			{
				auto pos = callback.get_playback_position();
				auto len = callback.get_track_length();

				g->DrawLine(pen_selection.get(),
					Gdiplus::Point((int)(pos * size.cx / len), 0),
					Gdiplus::Point((int)(pos * size.cx / len), size.cy));
				if (callback.is_seeking())
				{
					pos = callback.get_seek_position();
					g->DrawLine(pen_selection.get(),
						Gdiplus::Point((int)(pos * size.cx / len), 0),
						Gdiplus::Point((int)(pos * size.cx / len), size.cy));
				}
			}
			//Gdiplus::Graphics(dc).DrawImage(&bmp, 0, 0);
		}
		wnd.EndPaint(&ps);
	}

	void gdi_fallback_frontend::present()
	{}

	void gdi_fallback_frontend::on_state_changed(state s)
	{
#if 0
		if (s & state_size)
			update_size();
		if (s & state_color)
			update_effect_colors();
		if (s & state_position)
			update_effect_cursor();
		if (s & state_replaygain)
			update_replaygain();
#endif
		if (s & (state_data | state_size))
			update_data();
#if 0
		if (s & state_orientation)
			update_orientation();
		if (s & state_shade_played)
			update_shade_played();
#endif
	}

	void gdi_fallback_frontend::create_objects()
	{
		auto pen_from_color = [&](config::color color, scoped_ptr<Gdiplus::Pen>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new Gdiplus::Pen(Gdiplus::Color((BYTE)(c.a * 255), (BYTE)(c.r * 255), (BYTE)(c.g * 255), (BYTE)(c.b * 255))));
		};
		auto solid_brush_from_color = [&](config::color color, scoped_ptr<Gdiplus::SolidBrush>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new Gdiplus::SolidBrush(Gdiplus::Color((BYTE)(c.a * 255), (BYTE)(c.r * 255), (BYTE)(c.g * 255), (BYTE)(c.b * 255))));
		};
		pen_from_color(config::color_foreground, pen_foreground);
		pen_from_color(config::color_highlight, pen_highlight);
		pen_from_color(config::color_selection, pen_selection);
		solid_brush_from_color(config::color_background, brush_background);
	}

	void gdi_fallback_frontend::release_objects()
	{
		cached_bitmap.reset();
		pen_foreground.reset();
		pen_highlight.reset();
		pen_selection.reset();
		brush_background.reset();
	}

	void gdi_fallback_frontend::update_data()
	{
		CSize size = callback.get_size();
		Gdiplus::Bitmap bmp(size.cx, size.cy);
		scoped_ptr<Gdiplus::Graphics> buf(Gdiplus::Graphics::FromImage(&bmp));

		Gdiplus::Rect r(0, 0, size.cx, size.cy);
		buf->FillRectangle(brush_background.get(), r);

		service_ptr_t<waveform> w;
		if (callback.get_waveform(w))
		{
			pfc::list_hybrid_t<float, 2048> avg_min, avg_max, avg_rms;
			w->get_field("minimum", avg_min);
			w->get_field("maximum", avg_max);
			w->get_field("rms", avg_rms);
			for (size_t x = 0; x < (size_t)size.cx; ++x)
			{
				size_t ix = std::min(2047ul, x * 2048ul / size.cx);
				Gdiplus::Point p1(x, (int)(size.cy * (0.5 - avg_min[ix] * 0.5)));
				Gdiplus::Point p2(x, (int)(size.cy * (0.5 - avg_max[ix] * 0.5)));
				buf->DrawLine(pen_foreground.get(), p1, p2);
			}
		}

		Gdiplus::Graphics g(wnd.GetDC());
		cached_bitmap.reset(new Gdiplus::CachedBitmap(&bmp, &g));
	}
}