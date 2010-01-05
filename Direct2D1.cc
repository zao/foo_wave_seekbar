#include "PchSeekbar.h"
#include "Direct2D.h"
#include <boost/math/special_functions.hpp>

namespace wave
{
	bool has_direct2d1()
	{
		HMODULE lib = LoadLibrary(L"d2d1");
		FreeLibrary(lib);
		return !!lib;
	}

	direct2d1_frontend::direct2d1_frontend(HWND wnd, CSize size, visual_frontend_callback& callback)
		: cache_pump_work(new boost::asio::io_service::work(cache_pump)), cache_jobs(0), callback(callback)
	{
		cache_pump_thread.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, &cache_pump)));

		D2D1_FACTORY_OPTIONS opts = { D2D1_DEBUG_LEVEL_INFORMATION };
		D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, opts, &factory);
		D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, opts, &cache_factory);
		factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(wnd, D2D1::SizeU(size.cx, size.cy)), &rt);

		CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_ALL, __uuidof(IWICImagingFactory), (void**)&wic_factory);
		on_state_changed((state)~0);
	}

	direct2d1_frontend::~direct2d1_frontend()
	{
		cache_pump_work.reset();
		cache_pump_thread->join();
	}

	void direct2d1_frontend::clear()
	{
		color c = callback.get_color(config::color_background);
		clear_color = D2D1::ColorF(c.r, c.g, c.b, c.a);
	}

	void direct2d1_frontend::draw()
	{
		bool vertical = callback.get_orientation() == config::orientation_vertical;
		D2D1_SIZE_F size = rt->GetSize();
		if (vertical) std::swap(size.width, size.height);

		rt->BeginDraw();
		rt->Clear(clear_color);
		float seek_x = (float)(size.width * callback.get_seek_position() / callback.get_track_length());
		float play_x = (float)(size.width * callback.get_playback_position() / callback.get_track_length());
		
		if (vertical) rt->SetTransform(D2D1::Matrix3x2F(0, 1, 1, 0, 0, 0));
		{
			boost::mutex::scoped_lock sl(cache_mutex);
			if (cache_bitmap)
			{
				CComPtr<ID2D1Bitmap> bm;
				rt->CreateBitmapFromWicBitmap(cache_bitmap, &bm);
				rt->DrawBitmap(bm);
			}
		}
		if (callback.is_seeking())
			rt->DrawLine(D2D1::Point2F(seek_x), D2D1::Point2F(seek_x, size.height), brushes.selection_brush, 2.5f);
		if (callback.get_shade_played())
		{
			D2D1_COLOR_F hi = brushes.highlight_brush->GetColor();
			CComPtr<ID2D1SolidColorBrush> overlay_brush;
			rt->CreateSolidColorBrush(hi, D2D1::BrushProperties(0.3f), &overlay_brush);
			rt->FillRectangle(D2D1::RectF(0, 0, play_x, size.height), overlay_brush);
		}
		rt->DrawLine(D2D1::Point2F(play_x), D2D1::Point2F(play_x, size.height), brushes.selection_brush, 2.5f);
		rt->SetTransform(D2D1::Matrix3x2F::Identity());
		rt->EndDraw();
	}

	void direct2d1_frontend::present()
	{
	}

	void direct2d1_frontend::update_size()
	{
		CSize size = callback.get_size();
		rt->Resize(D2D1::SizeU(size.cx, size.cy));
		service_ptr_t<waveform> wf;
		if (callback.get_waveform(wf))
			trigger_texture_update(wf, size);
	}

	void direct2d1_frontend::trigger_texture_update(service_ptr_t<waveform> wf, CSize size)
	{
		boost::mutex::scoped_lock sl(cache_mutex);
		++cache_jobs;
		cache_pump.post(boost::bind(&direct2d1_frontend::update_texture_target, this, wf, D2D1::SizeF((float)size.cx, (float)size.cy)));
	}

	void direct2d1_frontend::on_state_changed(state s) {
		if (s & (state_size | state_orientation))
			update_size();
		if (s & state_color)
			regenerate_brushes();
		if (s & (state_data | state_color))
			update_data();
	}

	D2D1_POINT_2F round_point(D2D1_POINT_2F p)
	{
		p.x = boost::math::round(p.x);
		p.y = boost::math::round(p.y);
		return p;
	}

	void direct2d1_frontend::update_texture_target(service_ptr_t<waveform> wf, D2D1_SIZE_F target_size)
	{
		{
			boost::mutex::scoped_lock sl(cache_mutex);
			if (--cache_jobs)
				return;
		}
		
		if (callback.get_orientation() == config::orientation_vertical)
		{
			std::swap(target_size.width, target_size.height);
		}

		FLOAT dpi[2] = {};
		cache_factory->GetDesktopDpi(&dpi[0], &dpi[1]);
		D2D1_SIZE_F size = D2D1::SizeF(target_size.width * 96 / dpi[0], target_size.height * 96 / dpi[1]);

		D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(size.width, -size.height / 2.2f);

		pfc::list_t<float> mini, maxi, rms;
		wf->get_field("minimum", mini);
		wf->get_field("maximum", maxi);
		wf->get_field("rms", rms);

		CComPtr<ID2D1RenderTarget> temp_target;
		CComPtr<IWICBitmap> bm;
		wic_factory->CreateBitmap((UINT)target_size.width, (UINT)target_size.height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bm);
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), dpi[0], dpi[1]);
        cache_factory->CreateWicBitmapRenderTarget(bm, props, &temp_target);

		brush_set brushes = create_brush_set(temp_target);

		CComPtr<ID2D1PathGeometry> wave_geometry, rms_geometry;
		cache_factory->CreatePathGeometry(&wave_geometry);
		cache_factory->CreatePathGeometry(&rms_geometry);

		ID2D1RenderTarget* rt = temp_target;

		CComPtr<ID2D1GeometrySink> gs, rms_gs;
		wave_geometry->Open(&gs);
		size_t n = mini.get_size();

		// Prepare waveform
		gs->BeginFigure(D2D1::Point2F(), D2D1_FIGURE_BEGIN_HOLLOW);
		for (size_t i = 0; i < n; i += 1)
		{
			D2D1_POINT_2F p = D2D1::Point2(i / (float)n, maxi[i]);
			gs->AddLine(scale.TransformPoint(p));
		}
		for (size_t i = 0; i < n; i += 1)
		{
			int x = n - i - 1;
			D2D1_POINT_2F p = D2D1::Point2(x / (float)n, mini[x]);
			gs->AddLine(scale.TransformPoint(p));
		}
		gs->EndFigure(D2D1_FIGURE_END_CLOSED);
		gs->Close();

		// Prepare RMS
		rms_geometry->Open(&rms_gs);
		rms_gs->BeginFigure(D2D1::Point2F(), D2D1_FIGURE_BEGIN_HOLLOW);
		for (size_t i = 0; i < n; ++i)
		{
			D2D1_POINT_2F p = D2D1::Point2(i / (float)n, rms[i]);
			rms_gs->AddLine(scale.TransformPoint(p));
		}
		for (size_t i = 0; i < n; ++i)
		{
			int x = n - i - 1;
			D2D1_POINT_2F p = D2D1::Point2(x / (float)n, rms[x]);
			rms_gs->AddLine(scale.TransformPoint(p));
		}
		rms_gs->EndFigure(D2D1_FIGURE_END_CLOSED);
		rms_gs->Close();

		rt->BeginDraw();
		rt->Clear(clear_color);
		D2D1::Matrix3x2F centered = D2D1::Matrix3x2F::Translation(0.0f, boost::math::round(size.height / 2.0f));
		rt->SetTransform(centered);
		//rt->DrawLine(round_point(scale.TransformPoint(D2D1::Point2F(0.0f, -1.0f))), round_point(scale.TransformPoint(D2D1::Point2F(1.0f, -1.0f))), brushes.text_brush, 0.5);
		//rt->DrawLine(round_point(scale.TransformPoint(D2D1::Point2F(           ))), round_point(scale.TransformPoint(D2D1::Point2F(1.0f       ))), brushes.text_brush, 0.5);
		//rt->DrawLine(round_point(scale.TransformPoint(D2D1::Point2F(0.0f,  1.0f))), round_point(scale.TransformPoint(D2D1::Point2F(1.0f,  1.0f))), brushes.text_brush, 0.5);
		//rt->DrawGeometry(rms_geometry, brushes.highlight_brush, 0.5);
		rt->DrawGeometry(wave_geometry, brushes.foreground_brush);
		rt->EndDraw();

		boost::mutex::scoped_lock sl(cache_mutex);
		cache_bitmap = bm;
	}

	template <typename T>
	shared_ptr<pfc::list_t<T>> copy_list(const pfc::list_base_const_t<T>& l)
	{
		shared_ptr<pfc::list_t<T>> ret;
		ret->add_items(l);
		return ret;
	}

	void direct2d1_frontend::update_data()
	{
		D2D1_SIZE_F size = rt->GetSize();
		service_ptr_t<waveform> wf;
		if (callback.get_waveform(wf)) {
			trigger_texture_update(wf, callback.get_size());
		}
	}

	void direct2d1_frontend::regenerate_brushes()
	{
		CComPtr<ID2D1RenderTarget> rt_base = rt;
		brushes = create_brush_set(rt_base);
	}

	direct2d1_frontend::brush_set direct2d1_frontend::create_brush_set(CComPtr<ID2D1RenderTarget> target)
	{
		brush_set set;
#define RECREATE(Name) { color c = callback.get_color(config::color_##Name);\
	target->CreateSolidColorBrush(D2D1::ColorF(c.r, c.g, c.b, c.a), D2D1::BrushProperties(), &set.Name##_brush); }
		RECREATE(background)
		RECREATE(foreground)
		RECREATE(highlight)
		RECREATE(selection)
#undef RECREATE
		return set;
	}
}