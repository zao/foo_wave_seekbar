#pragma once
#include "frontend_sdk/VisualFrontend.h"
#include <memory>
#include <vector>

namespace wave
{
	struct frontend_module
	{
		frontend_module(HMODULE module, frontend_entrypoint* entry);
		~frontend_module();
		ref_ptr<visual_frontend> instantiate(config::frontend id, HWND wnd, wave::size size, visual_frontend_callback& callback, visual_frontend_config& conf);

	private:
		frontend_module(frontend_module const&);
		frontend_module& operator = (frontend_module const&);

	public:
		HMODULE module;
		frontend_entrypoint* entry;
	};

	void wait_for_frontend_module_load();
	std::vector<std::shared_ptr<frontend_module>> list_frontend_modules();
}