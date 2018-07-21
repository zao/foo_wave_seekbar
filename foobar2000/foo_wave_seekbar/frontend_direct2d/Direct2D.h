//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#pragma warning(disable : 4005)
#define _WIN32_WINNT 0x0600
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShellAPI.h>
#include <ObjBase.h>

#include <algorithm>
using std::max;
using std::min;
#include <memory>

#include "../frontend_sdk/VisualFrontend.h"
#include <D2D1.h>
#include <D2D1Helper.h>
#include <wincodec.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atomic>
#include <mutex>
#include <thread>

#include "../waveform_sdk/RefPointer.h"

namespace wave {
bool
has_direct2d1();

struct d2d_api
{
  HMODULE d2d_module = nullptr;
  using D2D1CreateFactoryFP =
    HRESULT(WINAPI *)(D2D1_FACTORY_TYPE factoryType,
                     REFIID riid,
                     CONST D2D1_FACTORY_OPTIONS* pFactoryOptions,
                     void** ppIFactory);
  D2D1CreateFactoryFP D2D1CreateFactory = nullptr;

	~d2d_api() {
		if (d2d_module) {
			FreeLibrary(d2d_module);
		}
	}
};

struct brush_set
{
  CComPtr<ID2D1SolidColorBrush> background_brush, foreground_brush,
    highlight_brush, selection_brush;
};

struct palette
{
  color background, foreground, highlight, selection;
};

brush_set
create_brush_set(ID2D1RenderTarget* target, palette pal);

struct image_cache
{
  explicit image_cache(d2d_api const& api);
  ~image_cache();
  void start();

  static void thread_func(void* data);
  void update_texture_target(ref_ptr<waveform> wf,
                             pfc::list_t<channel_info> infos,
                             D2D1_SIZE_F size,
                             bool vertical,
                             bool flip,
                             uint64_t serial);

  struct task_data
  {
    D2D1_SIZE_F size;
    uint64_t serial;
    ref_ptr<waveform> waveform;
    pfc::list_t<channel_info> infos;
    bool vertical;
    bool flipped;
  };

	d2d_api const& api;

  CComPtr<ID2D1Factory> factory;
  std::mutex mutex;
  std::condition_variable pump_alert;
  std::thread* pump_thread;
  std::deque<task_data> tasks;
  std::atomic<bool> should_terminate;

  CComPtr<IWICImagingFactory> wic_factory;
  CComPtr<IWICBitmap> last_bitmap;
  uint64_t bitmap_serial;
  palette colors;
};

struct direct2d1_frontend : visual_frontend
{
  direct2d1_frontend(HWND wnd,
                     wave::size size,
                     visual_frontend_callback& callback,
                     visual_frontend_config&);
  ~direct2d1_frontend();

  void clear();
  void draw();
  void present();
  void on_state_changed(state s);

  int get_present_interval() const { return 25; } // milliseconds

private:
  void regenerate_brushes();
  void trigger_texture_update(ref_ptr<waveform> wf, wave::size size);
  void update_data();
  void update_size();

	d2d_api api;

  visual_frontend_callback& callback;
  HWND wnd;

  FLOAT dpi[2];
  CComPtr<ID2D1Factory> factory;
  CComPtr<ID2D1HwndRenderTarget> rt;
  CComPtr<ID2D1Bitmap> wave_bitmap;
  uint64_t bitmap_serial;
  uint64_t last_serial_issued;

  std::unique_ptr<image_cache> cache;
  palette colors;

  brush_set brushes;
};
}
