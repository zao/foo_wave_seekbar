#include "PchSeekbar.h"
#include "FrontendLoader.h"
#include "GdiFallback.h"
#include "frontend_sdk/FrontendHelpers.h"
#include "util/Filesystem.h"
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

namespace wave
{
static boost::atomic<bool> modules_loaded;
static uv_mutex_t module_load_mutex;
static std::vector<boost::shared_ptr<frontend_module>> frontend_modules;
static uv_thread_t* load_thread;
static uv_barrier_t load_barrier;

void wait_for_frontend_module_load()
{
	if (!modules_loaded) {
		uv_mutex_lock(&module_load_mutex);
		if (load_thread) {
			uv_thread_join(load_thread);
			delete load_thread;
			load_thread = nullptr;
		}
		uv_mutex_unlock(&module_load_mutex);
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
	uv_barrier_init(&load_barrier, 2);
	load_thread = new uv_thread_t();
	uv_thread_create(load_thread, &dispatch_nullary_boost_function_pointer, new boost::function<void()>([]
	{
		auto* barrier = &load_barrier;
		modules_loaded = false;
		uv_mutex_lock(&module_load_mutex);
		uv_barrier_wait(barrier);
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
		uv_mutex_unlock(&module_load_mutex);
	}));
	uv_barrier_wait(&load_barrier);
}

struct frontend_module_init_stage : init_stage_callback
{
	void on_init_stage(t_uint32 stage) override
	{
		if (!core_api::is_quiet_mode_enabled()) {
			uv_mutex_init(&module_load_mutex);
			if (stage == init_stages::before_config_read)
				load_frontend_modules();
		}
	}
};
}

static service_factory_single_t<wave::frontend_module_init_stage> g_frontend_module_init_stage;