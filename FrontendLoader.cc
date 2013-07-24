#include "PchSeekbar.h"
#include "FrontendLoader.h"
#include "GdiFallback.h"
#include "util/Filesystem.h"
#include <atomic>

namespace wave
{
static std::atomic<bool> modules_loaded;
static asio::detail::mutex module_load_mutex;
static std::vector<std::shared_ptr<frontend_module>> frontend_modules;

void wait_for_frontend_module_load()
{
	if (!modules_loaded)
		asio::detail::scoped_lock<asio::detail::mutex> lk(module_load_mutex);
}

std::vector<std::shared_ptr<frontend_module>> list_frontend_modules()
{
	if (!modules_loaded)
		asio::detail::scoped_lock<asio::detail::mutex> lk(module_load_mutex);
	return frontend_modules;
}

frontend_module::frontend_module(HMODULE module, frontend_entrypoint* entry)
	: module(module), entry(entry)
{
}

frontend_module::~frontend_module()
{
	if (module) {
		FreeLibrary(module);
	}
}

static std::unique_ptr<asio::thread> loader_thread;
static void load_frontend_modules()
{
	HANDLE sync_point = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	loader_thread = std::make_unique<asio::thread>([&] {
		modules_loaded = false;
		asio::detail::scoped_lock<asio::detail::mutex> lk(module_load_mutex);
		SetEvent(sync_point);
		frontend_modules.push_back(std::make_shared<frontend_module>((HMODULE)0, g_gdi_entrypoint()));
		try {
			auto path = util::file_location_to_wide_path(core_api::get_my_full_path());
			auto directory = util::extract_directory_name(path);
			auto glob = directory + L"*.dll";
			util::enumerate_file_glob(glob, [&](WIN32_FIND_DATAW find_data)
			{
				auto entry = directory + find_data.cFileName;
				HMODULE lib = LoadLibraryW(entry.c_str());
				if (lib) {
					frontend_entrypoint_t entry = (frontend_entrypoint_t)GetProcAddress(lib, "g_seekbar_frontend_entrypoint");
					if (entry) {
						auto mod = std::make_shared<frontend_module>(lib, entry());
						frontend_modules.push_back(mod);
					}
					else {
						FreeLibrary(lib);
					}
				}
			});
		}
		catch (std::exception& e) {
			console::complain("Seekbar: couldn't load optional frontends", e);
		}
		modules_loaded = true;
	});
	WaitForSingleObject(sync_point, INFINITE);
	CloseHandle(sync_point);
}

struct frontend_module_init_stage : init_stage_callback
{
	void on_init_stage(t_uint32 stage) override
	{
		if (!core_api::is_quiet_mode_enabled()) {
			if (stage == init_stages::before_config_read)
				load_frontend_modules();
		}
	}
};

struct frontend_module_initquit : initquit
{
	void on_init() override {}

	void on_quit() override
	{
		if (loader_thread)
			loader_thread->join();
		loader_thread.reset();
	}
};
}

static service_factory_single_t<wave::frontend_module_init_stage> g_frontend_module_init_stage;
static initquit_factory_t<wave::frontend_module_initquit> g_frontend_module_initquit;