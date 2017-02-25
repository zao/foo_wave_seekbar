#include "PchSeekbar.h"
#include "FrontendLoader.h"
#include "GdiFallback.h"
#include "util/Filesystem.h"
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace wave
{
static std::atomic<unsigned> modules_loaded;
static std::mutex module_load_mutex;
static std::vector<std::shared_ptr<frontend_module>> frontend_modules;
static std::thread* load_thread;

void wait_for_frontend_module_load()
{
	if (!modules_loaded) {
		std::unique_lock<std::mutex> lk(module_load_mutex);
		if (load_thread) {
			load_thread->join();
			delete load_thread;
			load_thread = 0;
		}
	}
}

std::vector<std::shared_ptr<frontend_module>> list_frontend_modules()
{
	wait_for_frontend_module_load();
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

struct Candidate {
	std::wstring filename;
	std::vector<std::wstring> requirements;
};

static void load_frontend_modules()
{
  std::promise<void> load_barrier;
  auto load_block = load_barrier.get_future();
	load_thread = new std::thread([&load_barrier]
	{
		modules_loaded = false;
		std::unique_lock<std::mutex> lk(module_load_mutex);
    load_barrier.set_value();
		frontend_modules.push_back(std::make_shared<frontend_module>((HMODULE)0, g_gdi_entrypoint()));
		wchar_t const* filenames[] = { L"frontend_direct3d9.dll", L"frontend_direct2d.dll" };
		wchar_t const* dependencies[] = { L"d3d9.dll", L"d3dx9_42.dll", L"D3DCompiler_42.dll", L"d2d1.dll" };
		Candidate candidates[2];
		candidates[0].filename = filenames[0];
		candidates[0].requirements.assign(dependencies + 0, dependencies + 3);
		candidates[1].filename = filenames[1];
		candidates[1].requirements.assign(dependencies + 3, dependencies + 4);
		try {
			auto path = util::file_location_to_wide_path(core_api::get_my_full_path());
			auto directory = util::extract_directory_name(path);
			auto glob = directory + L"frontend_*.dll";
			util::enumerate_file_glob(glob, [&](WIN32_FIND_DATAW find_data)
			{
				auto entry = directory + find_data.cFileName;
				for (size_t i = 0; i < 2; ++i) {
					auto& candidate = candidates[i];
					if (find_data.cFileName == candidate.filename) {
						for (auto I = candidate.requirements.begin(); I != candidate.requirements.end(); ++I) {
							HMODULE lib = LoadLibraryW(I->c_str());
							if (!lib)
								return;
							FreeLibrary(lib);
						}
					}
				}
				HMODULE lib = LoadLibraryExW(entry.c_str(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
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
  load_block.get();
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
}

static service_factory_single_t<wave::frontend_module_init_stage> g_frontend_module_init_stage;