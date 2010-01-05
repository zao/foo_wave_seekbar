#pragma once
#include "VisualFrontend.h"
#include <GdiPlus.h>

namespace wave
{
	struct gdi_fallback_frontend : visual_frontend
	{
		gdi_fallback_frontend(HWND wnd, CSize size, visual_frontend_callback& callback);
		~gdi_fallback_frontend();

		void clear();
		void draw();
		void present();
		void on_state_changed(state s);

	private:
		void create_objects();
		void release_objects();
		void update_data();

		CWindow wnd;
		CSize size;

		ULONG_PTR gdiplus_token;
		scoped_ptr<Gdiplus::Pen> pen_foreground, pen_highlight, pen_selection;
		scoped_ptr<Gdiplus::SolidBrush> brush_background;
		scoped_ptr<Gdiplus::CachedBitmap> cached_bitmap;

		visual_frontend_callback& callback;
	};
}