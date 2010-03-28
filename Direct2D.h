#pragma once
#include "Direct3D.h"
#include <D2D1.h>
#include <D2D1Helper.h>
#include <wincodec.h>

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

	struct image_cache : enable_shared_from_this<image_cache>
	{
		image_cache();
		~image_cache();
		void start();

		void update_texture_target(service_ptr_t<waveform> wf, pfc::list_t<channel_info> infos, D2D1_SIZE_F size, bool vertical);

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
		direct2d1_frontend(HWND wnd, CSize size, visual_frontend_callback& callback);
		~direct2d1_frontend();

		void clear();
		void draw();
		void present();
		void on_state_changed(state s);

	private:
		void regenerate_brushes();
		void trigger_texture_update(service_ptr_t<waveform> wf, CSize size);
		void update_data();
		void update_size();

		CComPtr<ID2D1Factory> factory;
		CComPtr<ID2D1HwndRenderTarget> rt;
		
		shared_ptr<image_cache> cache;
		palette colors;

		brush_set brushes;

		visual_frontend_callback& callback;
	};
}