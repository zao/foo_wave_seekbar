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
				ShowWindow(SW_HIDE);
				SetParent(parent);
				host->relinquish_ownership(*this);
				host = new_host;

				SetWindowPos(0, position.x, position.y, position.cx, position.cy, SWP_NOZORDER);
			}
			else
			{
				host = new_host;
				CRect r;
				position.convert_to_rect(r);
				Create(parent, r, 0, WS_CHILD, settings.has_border ? WS_EX_STATICEDGE: 0);
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
	struct seekbar_uie : uie::container_atl_window<seekbar_window>
	{
		seekbar_uie();
		~seekbar_uie();

		virtual void set_config(stream_reader * p_reader, t_size p_size, abort_callback & p_abort);
		virtual void get_config(stream_writer * p_writer, abort_callback & p_abort) const;

		virtual const GUID& get_extension_guid() const;
		virtual void get_name(pfc::string_base& out) const;
		virtual void get_category(pfc::string_base& out) const;
		unsigned get_type() const;

		struct color_callback : cui::colours::common_callback
		{
			color_callback(seekbar_uie& parent);

			void on_colour_changed(t_size) const;
			void on_bool_changed(t_size) const;

			seekbar_uie& sb;
		} color_cb;

		static const GUID s_extension_guid;
	};
}