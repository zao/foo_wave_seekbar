//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#pragma warning(disable: 4005)
#define _WIN32_WINNT 0x600
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <boost/asio.hpp>
#include <ShellAPI.h>
#include <ObjBase.h>

#include <algorithm>
using std::min; using std::max;

#include "../frontend_sdk/VisualFrontend.h"
#include <D2D1.h>
#include <D2D1Helper.h>
#include <wincodec.h>
#include <atlbase.h>
#include <atlcom.h>
#include <boost/thread.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>

namespace wave
{
	bool has_direct2d1();

	struct brush_set {
		CComPtr<ID2D1SolidColorBrush> background_brush, foreground_brush, highlight_brush, selection_brush;
	};

	struct palette
	{
		color background, foreground, highlight, selection;
	};

	brush_set create_brush_set(CComPtr<ID2D1RenderTarget> target, palette pal);

	struct image_cache : boost::enable_shared_from_this<image_cache>
	{
		image_cache();
		~image_cache();
		void start();

		void update_texture_target(boost::shared_ptr<waveform::data> wf, pfc::list_t<channel_info> infos, D2D1_SIZE_F size, bool vertical, bool flip);

		CComPtr<ID2D1Factory> factory;
		boost::mutex mutex;
		boost::shared_ptr<boost::asio::io_service> pump;
		boost::scoped_ptr<boost::asio::io_service::work> pump_work;
		boost::scoped_ptr<boost::thread> pump_thread;
		size_t jobs;

		CComPtr<ID2D1HwndRenderTarget> rt;
		CComPtr<IWICImagingFactory> wic_factory;
		CComPtr<ID2D1Bitmap> wave_bitmap;
		palette colors;
	};

	struct direct2d1_frontend : visual_frontend
	{
		direct2d1_frontend(HWND wnd, wave::size size, visual_frontend_callback& callback, visual_frontend_config&);
		~direct2d1_frontend();

		void clear();
		void draw();
		void present();
		void on_state_changed(state s);

		int get_present_interval() const { return 25; } // milliseconds

	private:
		void regenerate_brushes();
		void trigger_texture_update(boost::shared_ptr<waveform::data> wf, wave::size size);
		void update_data();
		void update_size();

		CComPtr<ID2D1Factory> factory;
		CComPtr<ID2D1HwndRenderTarget> rt;
		
		boost::shared_ptr<image_cache> cache;
		palette colors;

		brush_set brushes;

		visual_frontend_callback& callback;
	};
}
