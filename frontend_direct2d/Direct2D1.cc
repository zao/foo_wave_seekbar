//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Direct2D.h"
#include "../frontend_sdk/FrontendHelpers.h"

std::function<void (std::function<void ()>)> in_main_thread;

namespace wave
{
	bool has_direct2d1()
	{
		HMODULE lib = LoadLibrary(L"d2d1");
		if (lib)
			FreeLibrary(lib);
		return !!lib;
	}

	D2D1_COLOR_F color_to_d2d1_color(color c)
	{
		return D2D1::ColorF(c.r, c.g, c.b, c.a);
	}

	struct create_d2d1_factory_func
	{
		create_d2d1_factory_func(D2D1_FACTORY_OPTIONS const& opts)
			: opts(opts)
		{}

		ID2D1Factory* operator () () const
		{
			ID2D1Factory* p = 0;
			D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, opts, &p);
			return p;
		}

		D2D1_FACTORY_OPTIONS const& opts;
	};
	
	D2D1_FACTORY_OPTIONS const opts = { };

	image_cache::image_cache()
		: should_terminate(false)
		, bitmap_serial(0)
	{
		D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, opts, &factory);
		CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_ALL, __uuidof(IWICImagingFactory), (void**)&wic_factory);
	}

	image_cache::~image_cache()
	{
		{
			boost::unique_lock<boost::mutex> lk(mutex);
			should_terminate = 1;
			pump_alert.notify_all();
		}
		pump_thread->join();
		delete pump_thread;
		tasks.clear();
	}

	void image_cache::start()
	{
		pump_thread = new boost::thread(&thread_func, this);
	}

	bool worker_task_ready(image_cache* self)
	{
		return self->should_terminate || self->tasks.size();
	}

	void image_cache::thread_func(void* data)
	{
		auto self = (image_cache*)data;
		while (1) {
			std::deque<task_data> ts;
			{
				boost::unique_lock<boost::mutex> lk(self->mutex);
				while (!worker_task_ready(self))
				{
					self->pump_alert.wait(lk);
				}
				if (self->should_terminate) return;
				std::swap(ts, self->tasks);
			}
			size_t max_index = 0u;
			task_data const* best = &ts.front();
			for (size_t i = 0; i < ts.size(); ++i)
			{
				auto& t = ts[i];
				if (t.serial > best->serial)
					best = &t;
			}
			task_data t = *best;
			self->update_texture_target(t.waveform, t.infos, t.size, t.vertical, t.flipped, t.serial);
		}
	}

	direct2d1_frontend::direct2d1_frontend(HWND wnd, wave::size size, visual_frontend_callback& callback, visual_frontend_config&)
		: callback(callback)
		, wnd(wnd)
		, bitmap_serial(0)
		, last_serial_issued(0)
	{
		in_main_thread = std::bind(&visual_frontend_callback::run_in_main_thread, &callback, std::placeholders::_1);
		factory.Attach(create_d2d1_factory_func(opts)());
		if (!factory)
			throw std::runtime_error("Direct2D not found. Ensure you're running Vista SP2 or later with the Platform Update pack.");

		cache.reset(new image_cache);
		cache->start();
	}

	direct2d1_frontend::~direct2d1_frontend()
	{
	}

	void direct2d1_frontend::clear()
	{
	}

	void direct2d1_frontend::draw()
	{
		bool vertical = callback.get_orientation() == config::orientation_vertical;
		bool flip = callback.get_flip_display();

		HRESULT hr = E_FAIL;
		while (! SUCCEEDED(hr))
		{
			if (! rt)
			{
				auto sz = callback.get_size();
				factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(wnd, D2D1::SizeU(sz.cx, sz.cy)), &rt);
				brushes = create_brush_set(rt, colors);
			}

			D2D1_SIZE_F size = rt->GetSize();
			if (vertical)
			{
				std::swap(size.width, size.height);
			}
		
			rt->BeginDraw();
			rt->Clear(color_to_d2d1_color(colors.background));
			float seek_x = (float)(size.width * callback.get_seek_position() / callback.get_track_length());
			float play_x = (float)(size.width * callback.get_playback_position() / callback.get_track_length());

			if (flip)
			{
				seek_x = size.width - seek_x;
				play_x = size.width - play_x;
			}
		
			if (vertical)
			{
				rt->SetTransform(D2D1::Matrix3x2F(0, 1, 1, 0, 0, 0));
			}

			{
				boost::unique_lock<boost::mutex> lk(cache->mutex);
				if (cache->last_bitmap && bitmap_serial < cache->bitmap_serial)
				{
					wave_bitmap.Release();
					rt->CreateBitmapFromWicBitmap(cache->last_bitmap, &wave_bitmap);
				}
				if (wave_bitmap)
				{
					rt->DrawBitmap(wave_bitmap);
				}
			}

			if (callback.is_seeking())
			{
				rt->DrawLine(D2D1::Point2F(seek_x), D2D1::Point2F(seek_x, size.height), brushes.selection_brush, 2.5f);
			}

			if (callback.get_shade_played())
			{
				D2D1_COLOR_F hi = brushes.highlight_brush->GetColor();
				CComPtr<ID2D1SolidColorBrush> overlay_brush;
				rt->CreateSolidColorBrush(hi, D2D1::BrushProperties(0.3f), &overlay_brush);
				if (flip)
					rt->FillRectangle(D2D1::RectF(play_x, 0, size.width, size.height), overlay_brush);
				else
					rt->FillRectangle(D2D1::RectF(0, 0, play_x, size.height), overlay_brush);
			}

			rt->DrawLine(D2D1::Point2F(play_x), D2D1::Point2F(play_x, size.height), brushes.selection_brush, 2.5f);
			rt->SetTransform(D2D1::Matrix3x2F::Identity());
			hr = rt->EndDraw();

			if (hr == D2DERR_RECREATE_TARGET)
			{
				brushes = brush_set();
				wave_bitmap.Release();
				rt.Release();
			}
		}
	}

	void direct2d1_frontend::present()
	{
	}

	void direct2d1_frontend::update_size()
	{
		auto size = callback.get_size();
		if (rt)
		{
			rt->Resize(D2D1::SizeU(size.cx, size.cy));
		}

		ref_ptr<waveform> wf;
		if (!callback.get_waveform(wf))
			wf = make_placeholder_waveform();
		trigger_texture_update(wf, size);
	}

	void direct2d1_frontend::trigger_texture_update(ref_ptr<waveform> wf, wave::size size)
	{
		boost::unique_lock<boost::mutex> lk(cache->mutex);
		switch (callback.get_downmix_display())
		{
		case config::downmix_mono:   if (wf->get_channel_count() > 1) wf = downmix_waveform(wf, 1); break;
		case config::downmix_stereo: if (wf->get_channel_count() > 2) wf = downmix_waveform(wf, 2); break;
		}
		pfc::list_t<channel_info> infos;
		callback.get_channel_infos(list_array_sink<channel_info>(infos));
		uint64_t serial = ++last_serial_issued;
		image_cache::task_data t;
		t.waveform = wf;
		t.infos = infos;
		t.size = D2D1::SizeF((float)size.cx, (float)size.cy);
		t.vertical = callback.get_orientation() == config::orientation_vertical;
		t.flipped = callback.get_flip_display();
		t.serial = serial;
		cache->tasks.push_back(t);
		cache->pump_alert.notify_all();
	}

	void direct2d1_frontend::on_state_changed(state s)
	{
		if (s & (state_size | state_orientation))
			update_size();
		if (s & state_color)
			regenerate_brushes();
		if (s & (state_data | state_color | state_channel_order | state_downmix_display | state_flip_display))
			update_data();
		InvalidateRect(wnd, nullptr, FALSE);
	}

	static float round(float v)
	{
		return v < 0 ? static_cast<float>(ceil(v - 0.5f)) : static_cast<float>(floor(v + 0.5f));
	}

	D2D1_POINT_2F round_point(D2D1_POINT_2F p)
	{
		p.x = round(p.x);
		p.y = round(p.y);
		return p;
	}

	void image_cache::update_texture_target(ref_ptr<waveform> wf, pfc::list_t<channel_info> infos, D2D1_SIZE_F target_size,
		bool vertical, bool flip, uint64_t serial)
	{
		if (vertical)
		{
			std::swap(target_size.width, target_size.height);
		}

		auto channel_numbers = expand_flags(wf->get_channel_map());
		pfc::list_t<int> channel_indices;
		infos.enumerate([&channel_indices, channel_numbers](channel_info const& info)
		{
			if (info.enabled)
			{
				auto I = std::find(channel_numbers.begin(), channel_numbers.end(), info.channel);
				decltype(I) first = channel_numbers.begin();
				if (I != channel_numbers.end())
				{
					channel_indices.add_item(std::distance(first, I));
				}				
			}
		});

		FLOAT dpi[2] = {};
		factory->GetDesktopDpi(&dpi[0], &dpi[1]);
		D2D1_SIZE_F size = D2D1::SizeF(target_size.width * 96 / dpi[0], target_size.height * 96 / dpi[1]);

		int index_count = channel_indices.get_count();
		D2D1::Matrix3x2F scale = D2D1::Matrix3x2F::Scale(size.width, -size.height / 2.5f / index_count);

		CComPtr<IWICBitmap> bm;
		wic_factory->CreateBitmap((UINT)target_size.width, (UINT)target_size.height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bm);
		HRESULT hr = E_FAIL;

		while (! SUCCEEDED(hr))
		{
			CComPtr<ID2D1RenderTarget> temp_target;
			D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat()); //, dpi[0], dpi[1]);
			factory->CreateWicBitmapRenderTarget(bm, props, &temp_target);

			brush_set brushes = create_brush_set(temp_target, colors);

			pfc::list_t<pfc::com_ptr_t<ID2D1PathGeometry>> wave_geometries, rms_geometries;
			auto& fac = factory;

			channel_indices.enumerate([&, fac, index_count](int index)
			{
				pfc::list_t<float> mini, maxi, rms;
				wf->get_field("minimum", index, list_array_sink<float>(mini));
				wf->get_field("maximum", index, list_array_sink<float>(maxi));
				wf->get_field("rms", index, list_array_sink<float>(rms));

				CComPtr<ID2D1PathGeometry> wave_geometry, rms_geometry;
				fac->CreatePathGeometry(&wave_geometry);
				fac->CreatePathGeometry(&rms_geometry);

				wave_geometries.add_item(wave_geometry);
				rms_geometries.add_item(rms_geometry);

				CComPtr<ID2D1GeometrySink> gs, rms_gs;
				wave_geometry->Open(&gs);
				size_t n = mini.get_size();

				// Prepare waveform
				size_t x;
				gs->BeginFigure(D2D1::Point2F(), D2D1_FIGURE_BEGIN_HOLLOW);
				for (size_t i = 0; i < n; i += 1)
				{
					if (flip)
						x = n - i - 1;
					else
						x = i;
					D2D1_POINT_2F p = D2D1::Point2(x / (float)n, maxi[x]);
					if (flip)
						p.x = 1.0f - p.x;
					gs->AddLine(scale.TransformPoint(p));
				}
				for (size_t i = 0; i < n; i += 1)
				{
					if (flip)
						x = i;
					else
						x = n - i - 1;
					D2D1_POINT_2F p = D2D1::Point2(x / (float)n, mini[x]);
					if (flip)
						p.x = 1.0f - p.x;
					gs->AddLine(scale.TransformPoint(p));
				}
				gs->EndFigure(D2D1_FIGURE_END_CLOSED);
				gs->Close();

				// Prepare RMS
				rms_geometry->Open(&rms_gs);
				rms_gs->BeginFigure(D2D1::Point2F(), D2D1_FIGURE_BEGIN_HOLLOW);
				for (size_t i = 0; i < n; ++i)
				{
					if (flip)
						x = n - i - 1;
					else
						x = i;
					D2D1_POINT_2F p = D2D1::Point2(x / (float)n, rms[x]);
					if (flip)
						p.x = 1.0f - p.x;
					rms_gs->AddLine(scale.TransformPoint(p));
				}
				for (size_t i = 0; i < n; ++i)
				{
					if (flip)
						x = i;
					else
						x = n - i - 1;
					D2D1_POINT_2F p = D2D1::Point2(x / (float)n, rms[x]);
					if (flip)
						p.x = 1.0f - p.x;
					rms_gs->AddLine(scale.TransformPoint(p));
				}
				rms_gs->EndFigure(D2D1_FIGURE_END_CLOSED);
				rms_gs->Close();
			});
		
			ID2D1RenderTarget* rt = temp_target;

			rt->BeginDraw();
			rt->Clear(color_to_d2d1_color(colors.background));
			int x = 0;
			wave_geometries.enumerate([this, &brushes, rt, size, &x, index_count](pfc::com_ptr_t<ID2D1PathGeometry> const& geom)
			{
				float offset = (float)(2*x + 1)/(float)(2*index_count);
				D2D1::Matrix3x2F centered = D2D1::Matrix3x2F::Translation(0.0f, round(offset*size.height));
				++x;

				rt->SetTransform(centered);
				rt->DrawGeometry(geom.get_ptr(), brushes.foreground_brush);
			});
			hr = rt->EndDraw();
		}

		image_cache* self = this;
		in_main_thread([self, bm, serial]()
		{
			boost::unique_lock<boost::mutex> lk(self->mutex);
			self->last_bitmap = bm;
			self->bitmap_serial = serial;
		});
	}

	template <typename T>
	std::shared_ptr<pfc::list_t<T>> copy_list(const pfc::list_base_const_t<T>& l)
	{
		std::shared_ptr<pfc::list_t<T>> ret;
		ret->add_items(l);
		return ret;
	}

	void direct2d1_frontend::update_data()
	{
		ref_ptr<waveform> wf;
		if (!callback.get_waveform(wf))
			wf = make_placeholder_waveform();
		trigger_texture_update(wf, callback.get_size());
	}

	void direct2d1_frontend::regenerate_brushes()
	{
		palette p =
		{
			callback.get_color(config::color_background),
			callback.get_color(config::color_foreground),
			callback.get_color(config::color_highlight),
			callback.get_color(config::color_selection)
		};
		colors = p;
		boost::unique_lock<boost::mutex> lk(cache->mutex);
		cache->colors = p;

		if (rt)
		{
			brushes = create_brush_set(rt, colors);
		}
	}

	brush_set create_brush_set(ID2D1RenderTarget* target, palette pal)
	{
		brush_set set;
#define RECREATE(Name) { color c = pal.Name;\
	target->CreateSolidColorBrush(D2D1::ColorF(c.r, c.g, c.b, c.a), D2D1::BrushProperties(), &set.Name##_brush); }
		RECREATE(background)
		RECREATE(foreground)
		RECREATE(highlight)
		RECREATE(selection)
#undef RECREATE
		return set;
	}
}
