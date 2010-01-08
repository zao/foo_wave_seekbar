#pragma once
#include "VisualFrontend.h"

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

		mem_dc(HDC src_dc, CSize size)
			: size(size)
		{
			CreateCompatibleDC(src_dc);
			bmp.CreateCompatibleBitmap(src_dc, size.cx, size.cy);
			old_bmp = SelectBitmap(bmp);
		}
		~mem_dc()
		{
			SelectBitmap(old_bmp);
		}
	};

	struct gdi_fallback_frontend : visual_frontend
	{
		gdi_fallback_frontend(HWND wnd, CSize, visual_frontend_callback& callback);
		~gdi_fallback_frontend();

		void clear();
		void draw();
		void present();
		void on_state_changed(state s);

	private:
		void create_objects();
		void release_objects();
		void update_data();

		CPoint orientate(CPoint);

		CWindow wnd;

		scoped_ptr<mem_dc> back_dc, wave_dc;
		scoped_ptr<mem_dc> shade_dc;
		scoped_ptr<CPen> pen_foreground, pen_highlight, pen_selection;
		scoped_ptr<CBrush> brush_background;

		visual_frontend_callback& callback;
	};
}