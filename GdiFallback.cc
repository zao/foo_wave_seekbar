#include "PchSeekbar.h"
#include "GdiFallback.h"

namespace wave
{
	inline Gdiplus::Color color_to_Color(color const& c)
	{
		return Gdiplus::Color((BYTE)(c.a * 255), (BYTE)(c.r * 255), (BYTE)(c.g * 255), (BYTE)(c.b * 255));
	}

	gdi_fallback_frontend::gdi_fallback_frontend(HWND wnd, CSize, visual_frontend_callback& callback)
		: wnd(wnd), callback(callback)
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
			CSize size = callback.get_size();
			if (callback.get_orientation() == config::orientation_vertical)
				std::swap(size.cx, size.cy);

			Gdiplus::Graphics gdc(dc);
			Gdiplus::Graphics* g = &gdc;

			auto draw_bar = [&](Gdiplus::Point p1, Gdiplus::Point p2)
			{
				g->DrawLine(pen_selection.get(), orientate(p1), orientate(p2));
			};

			if (cached_bitmap)
			{
				g->DrawCachedBitmap(cached_bitmap.get(), 0, 0);
			}
			
			auto pos = callback.get_playback_position();
			auto len = callback.get_track_length();
			
			if (callback.get_shade_played())
			{
				color c = callback.get_color(config::color_highlight);
				c.a = 0.3f;
				Gdiplus::SolidBrush shade(color_to_Color(c));
				Gdiplus::Point p = orientate(Gdiplus::Point((int)(pos * size.cx / len), size.cy));
				g->FillRectangle(&shade, 0, 0, p.X, p.Y);
			}

			if (callback.is_cursor_visible())
			{
				draw_bar(
					Gdiplus::Point((int)(pos * size.cx / len), 0),
					Gdiplus::Point((int)(pos * size.cx / len), size.cy));

				if (callback.is_seeking())
				{
					auto pos = callback.get_seek_position();
					draw_bar(
						Gdiplus::Point((int)(pos * size.cx / len), 0),
						Gdiplus::Point((int)(pos * size.cx / len), size.cy));
				}
			}
		}
		wnd.EndPaint(&ps);
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
		auto pen_from_color = [&](config::color color, scoped_ptr<Gdiplus::Pen>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new Gdiplus::Pen(color_to_Color(c)));
		};
		auto solid_brush_from_color = [&](config::color color, scoped_ptr<Gdiplus::SolidBrush>& out)
		{
			auto c = callback.get_color(color);
			out.reset(new Gdiplus::SolidBrush(color_to_Color(c)));
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
		Gdiplus::Rect r(0, 0, size.cx, size.cy);

		if (callback.get_orientation() == config::orientation_vertical)
			std::swap(size.cx, size.cy);

		scoped_ptr<Gdiplus::Graphics> buf(Gdiplus::Graphics::FromImage(&bmp));
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
				buf->DrawLine(pen_foreground.get(), orientate(p1), orientate(p2));
			}
		}

		Gdiplus::Graphics g(wnd.GetDC());
		cached_bitmap.reset(new Gdiplus::CachedBitmap(&bmp, &g));
	}

	Gdiplus::Point gdi_fallback_frontend::orientate(Gdiplus::Point p)
	{
		if (callback.get_orientation() == config::orientation_vertical)
			return Gdiplus::Point(p.Y, p.X);
		return p;
	}
}