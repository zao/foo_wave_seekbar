//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "SeekbarWindow.h"

namespace uie
{
	template <typename W, typename T = window>
	struct container_atl_window : W, T
	{
		window_host_ptr host;

		HWND create_or_transfer_window(HWND parent, const window_host_ptr& new_host, const ui_helpers::window_position_t& position)
		{
			if ((HWND)*this)
			{
				this->ShowWindow(SW_HIDE);
				this->SetParent(parent);
				host->relinquish_ownership(*this);
				host = new_host;

				this->SetWindowPos(0, position.x, position.y, position.cx, position.cy, SWP_NOZORDER);
			}
			else
			{
				host = new_host;
				CRect r;
				position.convert_to_rect(r);
				this->Create(parent, r, 0, WS_CHILD, this->settings.has_border ? WS_EX_STATICEDGE: 0);
			}

			return *this;
		}

		virtual void destroy_window()
		{
			::DestroyWindow(*this);
			host.release();
		}

		virtual bool is_available(const window_host_ptr& p) const
		{
			return true;
		}

		const window_host_ptr& get_host() const
		{
			return host;
		}

		virtual HWND get_wnd() const
		{
			return *this;
		}
	};
}

namespace wave
{
	struct seekbar_uie_base : uie::container_atl_window<seekbar_window>
	{
		seekbar_uie_base();
		~seekbar_uie_base();

		virtual void set_config(stream_reader * p_reader, t_size p_size, abort_callback & p_abort);
		virtual void get_config(stream_writer * p_writer, abort_callback & p_abort) const;

		virtual void get_name(pfc::string_base& out) const;

		struct color_callback : cui::colours::common_callback
		{
			color_callback(seekbar_uie_base& parent);

			void on_colour_changed(uint32_t) const;
			void on_bool_changed(uint32_t) const;

			seekbar_uie_base& sb;
		} color_cb;

	};

	extern const GUID s_panel_guid, s_toolbar_guid;
	
	template <uie::window_type_t WindowType>
	struct seekbar_uie_t : seekbar_uie_base
	{
		virtual void get_category(pfc::string_base& out) const
		{
			switch (WindowType)
			{
			case uie::type_toolbar:
				out.set_string("Toolbars"); return;
			case uie::type_panel:
			default:
				out.set_string("Panels"); return;
			}
		}

		virtual const GUID& get_extension_guid() const
		{
			switch (WindowType)
			{
			case uie::type_toolbar:
				return s_toolbar_guid;
			case uie::type_panel:
			default:
				return s_panel_guid;
			}
		}

		virtual unsigned get_type() const
		{
			return WindowType;
		}
	};
}
