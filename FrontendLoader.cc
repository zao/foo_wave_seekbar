#include "PchSeekbar.h"
#include "FrontendLoader.h"
#include "GdiFallback.h"
#include <boost/filesystem.hpp>
#include <thread>
#include <mutex>
#include <atomic>

namespace wave
{
static std::atomic<bool> modules_loaded;
static std::mutex module_load_mutex;
static boost::barrier module_load_barrier(2);
static std::vector<boost::shared_ptr<frontend_module>> frontend_modules;

void wait_for_frontend_module_load()
{
	if (!modules_loaded)
		std::lock_guard<std::mutex> lg(module_load_mutex);
}

std::vector<boost::shared_ptr<frontend_module>> list_frontend_modules()
{
	if (!modules_loaded)
		std::lock_guard<std::mutex> lg(module_load_mutex);
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

static boost::filesystem::path file_location_to_path(char const* fb2k_file)
{
	pfc::string8 native;

	// foobar2000_io::extract_native_path() can strip out file://, but will
	// currently (2011-08-14) break if fed a native path, thus this test.
	if (boost::algorithm::starts_with(fb2k_file, "file://"))
	{
		foobar2000_io::extract_native_path(fb2k_file, native);
	}
	else
	{
		native = fb2k_file;
	}

	native.replace_byte('/', '\\', 0);

	auto first = native.get_ptr(), last = first + native.get_length();		
	return boost::filesystem::path(first, last, utf8::utf8_codecvt_facet());
}

static void load_frontend_modules()
{
	std::thread t([] {
		modules_loaded = false;
		std::lock_guard<std::mutex> lg(module_load_mutex);
		module_load_barrier.wait();
		frontend_modules.push_back(boost::make_shared<frontend_module>((HMODULE)0, g_gdi_entrypoint()));
		try
		{
			namespace fs = boost::filesystem;
			boost::filesystem::path path = file_location_to_path(core_api::get_my_full_path());
			path = path.remove_filename();
			fs::directory_iterator I = fs::directory_iterator(path), last;
			for (; I != last; ++I)
			{
				fs::path p = *I;
				if (p.extension() != L".dll")
					continue;

				HMODULE lib = LoadLibraryW(p.wstring().c_str());
				if (lib)
				{
					frontend_entrypoint_t entry = (frontend_entrypoint_t)GetProcAddress(lib, "g_seekbar_frontend_entrypoint");
					if (entry)
					{
						auto mod = boost::make_shared<frontend_module>(lib, entry());
						frontend_modules.push_back(mod);
					}
					else
					{
						FreeLibrary(lib);
					}
				}
			}
		}
		catch (std::exception& e)
		{
			console::complain("Seekbar: couldn't load optional frontends", e);
		}
		modules_loaded = true;
	});
	t.detach();
	module_load_barrier.wait();
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