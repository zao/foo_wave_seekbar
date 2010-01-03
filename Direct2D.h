#pragma once
#include "Direct3D.h"
#include <D2D1.h>
#include <D2D1Helper.h>
#include <wincodec.h>

namespace wave
{
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
		void update_texture_target(service_ptr_t<waveform> wf, D2D1_SIZE_F size);
		void update_data();
		void update_size();

		CComPtr<ID2D1Factory> factory, cache_factory;
		CComPtr<ID2D1HwndRenderTarget> rt;

		CComPtr<IWICImagingFactory> wic_factory;
		CComPtr<IWICBitmap> cache_bitmap;
		
		boost::mutex cache_mutex;
		boost::asio::io_service cache_pump;
		boost::scoped_ptr<boost::asio::io_service::work> cache_pump_work;
		boost::scoped_ptr<boost::thread> cache_pump_thread;
		size_t cache_jobs;
		D2D1_COLOR_F clear_color;
		
		struct brush_set {
			CComPtr<ID2D1SolidColorBrush> background_brush, foreground_brush, highlight_brush, selection_brush;
		};

		brush_set create_brush_set(CComPtr<ID2D1RenderTarget> target);
		brush_set brushes;

		visual_frontend_callback& callback;
	};
}