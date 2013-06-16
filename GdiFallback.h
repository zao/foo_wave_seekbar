//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "frontend_sdk/VisualFrontend.h"

namespace wave
{
	struct mem_dc : CDC
	{
		CBitmap bmp;
		CBitmapHandle old_bmp;
		CSize size;

		explicit mem_dc(HDC src_dc)
		{
			CreateCompatibleDC(src_dc);
		}

		mem_dc(HDC src_dc, wave::size s)
		{
			CreateCompatibleDC(src_dc);
			bmp.CreateCompatibleBitmap(src_dc, s.cx, s.cy);
			old_bmp = SelectBitmap(bmp);
		}
		~mem_dc()
		{
			SelectBitmap(old_bmp);
		}
	};

	struct gdi_fallback_frontend : visual_frontend
	{
		gdi_fallback_frontend(HWND wnd, wave::size, visual_frontend_callback& callback, visual_frontend_config& conf);
		~gdi_fallback_frontend();

		void clear();
		void draw();
		void present();
		void on_state_changed(state s);

	private:
		void create_objects();
		void release_objects();
		void update_data();
		void update_positions();

		CPoint orientate(CPoint);

		CWindow wnd;
		boost::optional<CRect> last_play_rect;
		boost::optional<CRect> last_seek_rect;

		scoped_ptr<mem_dc> wave_dc, shaded_wave_dc;
		scoped_ptr<CPen> pen_foreground, pen_highlight, pen_selection;
		scoped_ptr<CBrush> brush_background;

		visual_frontend_callback& callback;
	};
}

extern "C" __declspec(dllexport) frontend_entrypoint* _cdecl g_gdi_entrypoint();