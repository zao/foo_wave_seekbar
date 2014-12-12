#include "PchSeekbar.h"
#include "FrontendLoader.h"
#include "GdiFallback.h"
#include "util/Filesystem.h"
#include <boost/atomic/atomic.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <vector>

namespace wave
{
static boost::atomic<unsigned> modules_loaded;
static boost::mutex module_load_mutex;
static std::vector<boost::shared_ptr<frontend_module>> frontend_modules;
static boost::thread* load_thread;
static boost::barrier load_barrier(2);

void wait_for_frontend_module_load()
{
	if (!modules_loaded) {
		boost::unique_lock<boost::mutex> lk(module_load_mutex);
		if (load_thread) {
			load_thread->join();
			delete load_thread;
			load_thread = 0;
		}
	}
}

std::vector<boost::shared_ptr<frontend_module>> list_frontend_modules()
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
	// TODO(zao): Load frontend modules and all that jazz
	/*
	load_thread = new boost::thread([](void* data)
	{
		modules_loaded = false;
		boost::unique_lock<boost::mutex> lk(module_load_mutex);
		load_barrier.wait();
		frontend_modules.push_back(boost::make_shared<frontend_module>((HMODULE)0, g_gdi_entrypoint()));
		Candidate candidates[] = {
			{ L"frontend_direct3d9.dll", { L"d3d9.dll", L"d3dx9_42.dll", L"D3DCompiler_42.dll" } },
			{ L"frontend_direct2d.dll", { L"d2d1.dll" } } };
		try {
			auto path = util::file_location_to_wide_path(core_api::get_my_full_path());
			auto directory = util::extract_directory_name(path);
			auto glob = directory + L"frontend_*.dll";
			util::enumerate_file_glob(glob, [&](WIN32_FIND_DATAW find_data)
			{
				auto entry = directory + find_data.cFileName;
				for (auto candidate : candidates) {
					if (find_data.cFileName == candidate.filename) {
						for (auto req : candidate.requirements) {
							HMODULE lib = LoadLibraryW(req.c_str());
							if (!lib)
								return;
							FreeLibrary(lib);
						}
					}
				}
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
	load_barrier.wait();
	*/
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