#pragma once
#include "frontend_sdk/VisualFrontend.h"

namespace wave
{
	struct frontend_module : noncopyable
	{
		frontend_module(HMODULE module, frontend_entrypoint* entry);
		~frontend_module();
		ref_ptr<visual_frontend> instantiate(config::frontend id, HWND wnd, wave::size size, visual_frontend_callback& callback, visual_frontend_config& conf);

		HMODULE module;
		frontend_entrypoint* entry;
	};

	void wait_for_frontend_module_load();
	std::vector<boost::shared_ptr<frontend_module>> list_frontend_modules();
}