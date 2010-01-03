#pragma once
#include "SeekbarWindow.h"

namespace wave
{
	struct seekbar_dui : ui_element_instance, seekbar_window
	{
		static GUID g_get_guid()
		{
			return s_guid;
		}

		static GUID g_get_subclass()
		{
			return ui_element_subclass_utility;
		}

		static void g_get_name(pfc::string_base & out)
		{
			out = "Waveform Seekbar";
		}

		static char const * g_get_description()
		{
			return "A seekbar with the waveform of the track as background.";
		}
		
		static ui_element_config::ptr g_get_default_configuration();
		void set_configuration(ui_element_config::ptr data);
		ui_element_config::ptr get_configuration();

		void initialize_window(HWND parent)
		{
			Create(parent, 0, 0, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, settings.has_border ? WS_EX_STATICEDGE : 0);
		}

		void notify(GUID const & what, t_size param1, void const * param2, t_size param2Size);
		seekbar_dui(ui_element_config::ptr cfg, ui_element_instance_callback::ptr callback);
		~seekbar_dui();

	protected:
		virtual void get_colors();
		virtual bool forward_rightclick() { return callback->is_edit_mode_enabled(); }
		virtual bool edit_mode_context_menu_test(const POINT & p_point,bool p_fromkeyboard) {return true;}
		static GUID const s_guid;
		ui_element_instance_callback::ptr callback;
	};
}