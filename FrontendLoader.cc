#include "PchSeekbar.h"
#include "FrontendLoader.h"
#include "GdiFallback.h"
#include "frontend_sdk/FrontendHelpers.h"
#include "util/Filesystem.h"
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace wave
{
static boost::atomic<bool> modules_loaded;
static boost::mutex module_load_mutex;
static std::vector<boost::shared_ptr<frontend_module>> frontend_modules;
static std::unique_ptr<boost::thread> load_thread;
static boost::barrier load_barrier(2);

void wait_for_frontend_module_load()
{
	if (!modules_loaded) {
		boost::unique_lock<boost::mutex> lk(module_load_mutex);
		if (load_thread) {
			load_thread->join();
			load_thread.reset();
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

struct CandidateSet {
	std::vector<Candidate> candidates;

	CandidateSet() {
		{
			Candidate d3d = { L"frontend_direct3d9.dll" };
			wchar_t const* d3d_requirements[] = { L"d3d9.dll", L"d3dx9_42.dll", L"D3DCompiler_42.dll" };
			d3d.requirements.assign(array_begin(d3d_requirements), array_end(d3d_requirements));
			candidates.push_back(d3d);
		}
		{
			Candidate d2d = { L"frontend_direct2d.dll" };
			wchar_t const* d2d_requirements[] = { L"d2d1.dll" };
			d2d.requirements.assign(array_begin(d2d_requirements), array_end(d2d_requirements));
			candidates.push_back(d2d);
		}
	}

	Candidate const* match(wchar_t const* other) const {
		for (size_t i = 0; i < candidates.size(); ++i) {
			if (candidates[i].filename == other) {
				return &candidates[i];
			}
		}
		return NULL;
	}
};

static void load_frontend_modules()
{
	load_thread.reset(new boost::thread([]
	{
		auto* barrier = &load_barrier;
		modules_loaded = false;
		boost::unique_lock<boost::mutex> lk(module_load_mutex);
		load_barrier.wait();
		frontend_modules.push_back(boost::make_shared<frontend_module>((HMODULE)0, g_gdi_entrypoint()));
		CandidateSet candidates;
		try {
			auto path = util::file_location_to_wide_path(core_api::get_my_full_path());
			auto directory = util::extract_directory_name(path);
			auto glob = directory + L"frontend_*.dll";
			util::enumerate_file_glob(glob, [&](WIN32_FIND_DATAW find_data)
			{
				auto entry = directory + find_data.cFileName;
				if (Candidate const* candidate = candidates.match(find_data.cFileName)) {
					for (size_t i = 0; i < candidate->requirements.size(); ++i) {
						HMODULE lib = LoadLibraryW(candidate->requirements[i].c_str());
						if (!lib)
							return;
						FreeLibrary(lib);
					}
				}
				HMODULE lib = LoadLibraryW(entry.c_str());
				if (lib) {
					frontend_entrypoint_t entry = (frontend_entrypoint_t)GetProcAddress(lib, "g_seekbar_frontend_entrypoint");
					if (entry) {
						auto mod = boost::make_shared<frontend_module>(lib, entry());
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
	}));
	load_barrier.wait();
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