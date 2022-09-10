//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "SeekCallback.h"

namespace wave {
struct seek_tooltip : seek_callback
{
  public:
    explicit seek_tooltip(HWND parent)
      : parent(parent)
    {
        tooltip.Create(nullptr);
        toolinfo.cbSize = sizeof(toolinfo);
        toolinfo.uFlags =
          TTF_TRACK | TTF_IDISHWND | TTF_ABSOLUTE | TTF_TRANSPARENT;
        toolinfo.hwnd = parent;
        toolinfo.uId = 0;
        static wchar_t empty_label[1] = L"";
        toolinfo.lpszText = empty_label;
        toolinfo.hinst = core_api::get_my_instance();
        tooltip.AddTool(&toolinfo);
    }

    ~seek_tooltip()
    {
        tooltip.DelTool(&toolinfo);
        tooltip.DestroyWindow();
    }

    virtual void on_seek_begin() override
    {
        show = true;
        track_mouse();
    }

    virtual void on_seek_position(double time, bool legal) override
    {
        show = legal;
        std::wstring txt = format_time(time);
        toolinfo.lpszText = const_cast<wchar_t*>(txt.c_str());
        tooltip.SetToolInfo(&toolinfo);
        track_mouse();
    }

    virtual void on_seek_end(bool aborted) override
    {
        show = false;
        tooltip.TrackActivate(&toolinfo, FALSE);
    }

  private:
    seek_tooltip(seek_tooltip const&);
    seek_tooltip& operator=(seek_tooltip const&);

  private:
    CToolTipCtrl tooltip;
    TOOLINFO toolinfo;
    CWindow parent;
    bool show;

    void track_mouse()
    {
        POINT pos = {};
        GetCursorPos(&pos);
        tooltip.TrackPosition(pos.x + 10, pos.y - 20);
        tooltip.TrackActivate(&toolinfo, show ? TRUE : FALSE);
    }

    std::wstring format_time(double time)
    {
        auto str = pfc::stringcvt::string_os_from_utf8(
          pfc::format_time(pfc::rint64(time)));
        std::wstring out = str.get_ptr();
        return out;
    }
};
}
