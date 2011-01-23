#pragma once
#include "VisualFrontend.h"

namespace wave
{
	namespace direct3d11
	{
		extern const GUID guid_fx11_string;

		struct frontend_impl : visual_frontend
		{
			frontend_impl(HWND wnd, CSize client_size, visual_frontend_callback& callback, visual_frontend_config& conf);
			virtual void clear();
			virtual void draw();
			virtual void present();
			virtual void on_state_changed(state s);

		private:
			visual_frontend_callback& callback;
			visual_frontend_config& conf;
		};
	};
}