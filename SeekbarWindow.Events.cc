//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarWindow.h"
#include "SeekTooltip.h"
#include "Clipboard.h"

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
		default:
			return;
		}
	}
}