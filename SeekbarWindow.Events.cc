//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarWindow.h"
#include "SeekTooltip.h"
#include "Clipboard.h"

#include <wincodec.h>
#pragma comment(lib, "windowscodecs")

// {EBEABA3F-7A8E-4A54-A902-3DCF716E6A97}
static const GUID guid_seekbar_branch =
{ 0xebeaba3f, 0x7a8e, 0x4a54, { 0xa9, 0x2, 0x3d, 0xcf, 0x71, 0x6e, 0x6a, 0x97 } };

// {E913D7C7-676A-4A4F-A0F4-3DA33622D3D8}
static const GUID guid_seekbar_screenshot_branch =
{ 0xe913d7c7, 0x676a, 0x4a4f, { 0xa0, 0xf4, 0x3d, 0xa3, 0x36, 0x22, 0xd3, 0xd8 } };

// {FA3261D7-671B-4BE4-AA18-75B8EFA7E4D6}
static const GUID guid_seekbar_screenshot_width =
{ 0xfa3261d7, 0x671b, 0x4be4, { 0xaa, 0x18, 0x75, 0xb8, 0xef, 0xa7, 0xe4, 0xd6 } };

// {B3534AC9-609A-4A6E-8B6A-7522D7E9E419}
static const GUID guid_seekbar_screenshot_height =
{ 0xb3534ac9, 0x609a, 0x4a6e, { 0x8b, 0x6a, 0x75, 0x22, 0xd7, 0xe9, 0xe4, 0x19 } };

// {0936311D-C065-4C72-806C-1F68403B385D}
static const GUID guid_seekbar_screenshot_filename_format =
{ 0x936311d, 0xc065, 0x4c72, { 0x80, 0x6c, 0x1f, 0x68, 0x40, 0x3b, 0x38, 0x5d } };

static advconfig_branch_factory g_seekbar_screenshot_branch("Screenshots", guid_seekbar_screenshot_branch, guid_seekbar_branch, 0.0);

static advconfig_integer_factory g_seekbar_screenshot_width ("Horizontal size (pixels)", guid_seekbar_screenshot_width,  guid_seekbar_screenshot_branch, 0.1, 1024, 16, 8192);
static advconfig_integer_factory g_seekbar_screenshot_height("Vertical size (pixels)",   guid_seekbar_screenshot_height, guid_seekbar_screenshot_branch, 0.2, 1024, 16, 8192);
static advconfig_string_factory g_seekbar_screenshot_filename_format("File format template (either absolute path+filename or just filename)", guid_seekbar_screenshot_filename_format, guid_seekbar_screenshot_branch, 0.3, "%artist% - %tracknumber%. %title%.png");

static bool is_outside(CPoint point, CRect r, int N, bool horizontal)
{
	if (!horizontal)
	{
		std::swap(point.x, point.y);
		std::swap(r.right, r.bottom);
		std::swap(r.left, r.top);
	}
	return point.y < -2 * N || point.y > r.bottom - r.top + 2 * N ||
		point.x < -N     || point.x > r.right - r.left + N;
}

struct menu_item_info
{
	menu_item_info(UINT f_mask, UINT f_type, UINT f_state, LPTSTR dw_type_data, UINT w_id, HMENU h_submenu = 0, HBITMAP hbmp_checked = 0, HBITMAP hbmp_unchecked = 0, ULONG_PTR dw_item_data = 0, HBITMAP hbmp_item = 0)
	{
		MENUITEMINFO mi =
		{
			sizeof(mi), f_mask | MIIM_TYPE | MIIM_STATE | MIIM_ID, f_type, f_state, w_id, h_submenu, hbmp_checked, hbmp_unchecked, dw_item_data, dw_type_data, _tcslen(dw_type_data), hbmp_item
		};
		this->mi = mi;
	}
	mutable MENUITEMINFO mi;
	operator LPMENUITEMINFO () const
	{
		return &mi;
	}
};

namespace wave
{
	void seekbar_window::on_wm_paint(HDC dc)
	{
		GetClientRect(client_rect);
		if (!(client_rect.right > 1 && client_rect.bottom > 1))
		{
			ValidateRect(0);
			return;
		}

		if (!fe->frontend && !initializing_graphics)
		{
			initialize_frontend();
		}

		if (fe->frontend)
		{
			fe->frontend->clear();
			fe->frontend->draw();
			fe->frontend->present();
		}

		ValidateRect(0);
	}

	void seekbar_window::on_wm_size(UINT wparam, CSize size)
	{
		if (size.cx < 1 || size.cy < 1)
			return;
		set_orientation(size.cx >= size.cy
			? config::orientation_horizontal
			: config::orientation_vertical);

		scoped_lock sl(fe->mutex);
		fe->callback->set_size(wave::size(size.cx, size.cy));
		if (fe->frontend)
			fe->frontend->on_state_changed((visual_frontend::state)(visual_frontend::state_size | visual_frontend::state_orientation));
		repaint();
	}

	void seekbar_window::on_wm_timer(UINT_PTR wparam)
	{
		if (wparam == REPAINT_TIMER_ID && core_api::are_services_available())
		{
			scoped_lock sl(fe->mutex);
			static_api_ptr_t<playback_control> pc;
			double t = pc->playback_get_position();
			set_cursor_position((float)t);
			if (fe->frontend)
				fe->frontend->on_state_changed(visual_frontend::state_position);
			repaint();
		}
	}

	void seekbar_window::on_wm_destroy()
	{
		if (repaint_timer_id)
			KillTimer(repaint_timer_id);
		repaint_timer_id = 0;

		scoped_lock sl(fe->mutex);
		fe->clear();
	}

	LRESULT seekbar_window::on_wm_erasebkgnd(HDC dc)
	{
		return 1;
	}

	void seekbar_window::on_wm_lbuttondown(UINT wparam, CPoint point)
	{
		drag_state = (wparam & MK_CONTROL) ? MouseDragSelection : MouseDragSeeking;
		if (!tooltip)
		{
			tooltip.reset(new seek_tooltip(*this));
			seek_callbacks += tooltip;
		}

		scoped_lock sl(fe->mutex);
		if (drag_state == MouseDragSeeking)
		{
			fe->callback->set_seeking(true);

			for each(auto cb in seek_callbacks)
				if (auto p = cb.lock())
					p->on_seek_begin();

			set_seek_position(point);
			if (fe->frontend)
			{
				fe->frontend->on_state_changed(visual_frontend::state_position);
			}
		}
		else
		{
			drag_data.to = drag_data.from = compute_position(point);
		}

		SetCapture();
		repaint();
	}

	void seekbar_window::on_wm_lbuttonup(UINT wparam, CPoint point)
	{
		scoped_lock sl(fe->mutex);
		ReleaseCapture();
		if (drag_state == MouseDragSeeking)
		{
			bool completed = fe->callback->is_seeking();
			if (completed)
			{
				fe->callback->set_seeking(false);
				set_seek_position(point);
				if (fe->frontend)
					fe->frontend->on_state_changed(visual_frontend::state_position);
				repaint();
				static_api_ptr_t<playback_control> pc;
				pc->playback_seek(fe->callback->get_seek_position());
			}
			for each(auto cb in seek_callbacks)
				if (auto p = cb.lock())
					p->on_seek_end(!completed);
		}
		else if (drag_state == MouseDragSelection)
		{
			auto source = fe->displayed_song;
			if (source.is_valid() && drag_data.to >= 0)
			{
				drag_data.to = compute_position(point);

				auto from = std::min(drag_data.from, drag_data.to);
				auto to = std::max(drag_data.from, drag_data.to);
			
				clipboard::render_audio(source, from, to);
			}
		}
		drag_state = MouseDragNone;
	}

	void seekbar_window::on_wm_mousemove(UINT wparam, CPoint point)
	{
		if (drag_state != MouseDragNone)
		{
			if (last_seek_point == point)
				return;

			last_seek_point = point;

			scoped_lock sl(fe->mutex);
			CRect r;
			GetWindowRect(r);
			int const N = 40;
			bool horizontal = fe->callback->get_orientation() == config::orientation_horizontal;

			bool outside = is_outside(point, r, N, horizontal);

			if (drag_state == MouseDragSeeking)
			{
				fe->callback->set_seeking(!outside);

				set_seek_position(point);
				if (fe->frontend)
				{
					fe->frontend->on_state_changed(visual_frontend::state_position);
				}
				repaint();
			}
			else if (drag_state == MouseDragSelection)
			{
				drag_data.to = outside
					? -1.0f
					: compute_position(point);
			}
		}
	}

  struct screenshot_saver : screenshot_settings
  {
    screenshot_saver(int width, int height)
    {
      this->width = width;
      this->height = height;
      this->context = this;
      this->write_screenshot = &save_shot_trampoline;
      this->flags = 0u;
    }

    void save_shot(BYTE const* pixels)
    {
      CComPtr<IWICImagingFactory> wic_factory;
      HRESULT hr;
      hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr,
            CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
            (void**)&wic_factory);

      WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppRGBA;
      GUID container_format = GUID_ContainerFormatPng;

      abort_callback_dummy cb;

      try
      {
        filesystem::g_open_tempmem(target_file, cb);
      }
      catch (std::exception& e)
      {
        console::error(e.what());
        return;
      }

      CComPtr<IWICStream> wic_stream;
      hr = wic_factory->CreateStream(&wic_stream);
      CComPtr<IStream> mem_stream;
      hr = CreateStreamOnHGlobal(NULL, TRUE, &mem_stream);
      hr = wic_stream->InitializeFromIStream(mem_stream);

      {
        CComPtr<IWICBitmapEncoder> encoder;
        hr = wic_factory->CreateEncoder(container_format, nullptr, &encoder);
        hr = encoder->Initialize(wic_stream, WICBitmapEncoderNoCache);

        CComPtr<IWICBitmapFrameEncode> frame_encode;
        hr = encoder->CreateNewFrame(&frame_encode, nullptr);
        hr = frame_encode->Initialize(nullptr);
        hr = frame_encode->SetSize(width, height);
        
        hr = frame_encode->SetPixelFormat(&pixel_format);
        hr = frame_encode->WritePixels(height, width*4, height*width*4, (BYTE*)pixels);
        hr = frame_encode->Commit();
        hr = encoder->Commit();
      }

      LARGE_INTEGER pos;
      pos.QuadPart = 0;
      hr = mem_stream->Seek(pos, STREAM_SEEK_SET, NULL);

      uint8_t buf[4096];
      ULONG amount_read = 0;
      while (SUCCEEDED(hr = mem_stream->Read(buf, sizeof(buf), &amount_read)) && amount_read > 0)
      {
        target_file->write(buf, amount_read, cb);
      }
    }

    file_ptr target_file;

    static void save_shot_trampoline(void* self, BYTE const* pixels) { auto p = (screenshot_saver*)self; p->save_shot(pixels); }
  };

  struct file_uri_builder
  {
    file_uri_builder(pfc::string8 src)
      : protocol("file://")
    {
      t_size n = std::min(protocol.get_length(), src.get_length());
      if (strnicmp(protocol, src, n) == 0)
      {
        src = src + protocol.get_length();
      }

      t_size filename_offset = src.scan_filename();
      directory.set_string(src, filename_offset);
      filename.set_string(src + filename_offset);
    }

    operator pfc::string8 () const
    {
      pfc::string8 ret;
      ret.add_string(protocol);
      ret.add_string(directory);
      ret.add_string(filename);
      return ret;
    }

    pfc::string8 protocol;
    pfc::string8 directory;
    pfc::string8 filename;
  };

	void seekbar_window::on_wm_rbuttonup(UINT wparam, CPoint point)
	{
		if (forward_rightclick())
		{
			SetMsgHandled(FALSE);
			return;
		}

		WTL::CMenu m;
		m.CreatePopupMenu();
		m.InsertMenu(-1, MF_BYPOSITION | MF_STRING, 3, L"Configure");
    m.InsertMenu(-1, MF_BYPOSITION | MF_STRING, 4, L"Capture screenshot");
		ClientToScreen(&point);
		BOOL ans = m.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, *this, 0);
		config::frontend old_kind = settings.active_frontend_kind;
		switch(ans)
		{
		case 3:
			if (config_dialog)
				config_dialog->BringWindowToTop();
			else
			{
				config_dialog.reset(new configuration_dialog(*this));
				config_dialog->Create(*this);
			}
			break;
    case 4:
      {
        if (fe && fe->frontend)
        {
          metadb_handle_ptr h;
          titleformat_object::ptr tf_obj;

          {
            static_api_ptr_t<titleformat_compiler> tfc;
            pfc::string8 format;
            g_seekbar_screenshot_filename_format.get(format);
            if (!tfc->compile(tf_obj, format.get_ptr()))
            {
              console::error("Could not take screenshot, invalid filename format specified.");
              return;
            }
            
            playable_location_impl loc;
            fe->callback->get_playable_location(loc);
            static_api_ptr_t<metadb>()->handle_create(h, loc);
          }

          pfc::string8 formatted;
          if (!h->format_title(nullptr, formatted, tf_obj, nullptr))
          {
            console::error("Could not format filename for screenshot, file information not available yet.");
            return;
          }
          
          abort_callback_dummy cb;

          // cases:
          // format is a full path: use as-is
          // format is a filename: put in My Pictures

          file_uri_builder uri = formatted;

          if (uri.directory.is_empty())
          {
            wchar_t pictures[MAX_PATH+1] = {};
            SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, pictures);
            uri.directory = pfc::stringcvt::string_utf8_from_wide(pictures);
            uri.directory.add_char('\\');
          }

          pfc::string8 target_filename = uri;
          
          screenshot_saver saver(
            (int)g_seekbar_screenshot_width,
            (int)g_seekbar_screenshot_height);
          fe->frontend->make_screenshot(&saver);
          if (saver.target_file.is_valid())
          {
            file_ptr target;
            try
            {
              filesystem::g_open_write_new(target, target_filename, cb);
              file::g_transfer_file(saver.target_file, target, cb);
            }
            catch (std::exception& e)
            {
              console::error(e.what());
              return;
            }
          }
        }
      }
		default:
			return;
		}
	}
}