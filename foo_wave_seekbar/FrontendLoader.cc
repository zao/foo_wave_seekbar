#include "FrontendLoader.h"
#include <SDK/initquit.h>
#include "GdiFallback.h"
#include "util/Barrier.h"
#include "util/Filesystem.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace wave {
static std::atomic<unsigned> modules_loaded;
static std::mutex module_load_mutex;
static std::vector<std::shared_ptr<frontend_module>> frontend_modules;
static std::thread* load_thread;
static util::barrier load_barrier(2);

void
wait_for_frontend_module_load()
{
    if (!modules_loaded) {
        std::unique_lock<std::mutex> lk(module_load_mutex);
        if (load_thread) {
            load_thread->join();
            delete load_thread;
            load_thread = nullptr;
        }
    }
}

std::vector<std::shared_ptr<frontend_module>>
list_frontend_modules()
{
    wait_for_frontend_module_load();
    return frontend_modules;
}

frontend_module::frontend_module(HMODULE module, frontend_entrypoint* entry)
  : module(module)
  , entry(entry)
{}

frontend_module::~frontend_module()
{
    if (module) {
        FreeLibrary(module);
    }
}

struct Candidate
{
    std::wstring filename;
    std::vector<std::wstring> requirements;
};

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT_DECL(g_gdi_entrypoint);
FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT_DECL(g_direct2d_entrypoint);
// FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_NAMED_ENTRYPOINT_DECL(g_direct3d9_entrypoint);

static void
load_frontend_modules()
{
    // TODO(zao): Load frontend modules and all that jazz
    load_thread = new std::thread([] {
        modules_loaded = false;
        std::unique_lock<std::mutex> lk(module_load_mutex);
        load_barrier.wait();
        HMODULE own_module;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)load_frontend_modules,
                           &own_module);
        {
            frontend_entrypoint* gdi_impl = g_gdi_entrypoint();
            frontend_modules.push_back(
              std::make_shared<frontend_module>(static_cast<HMODULE>(nullptr), gdi_impl));
        }

        {
            frontend_entrypoint_t d3d9_entrypoint =
              (frontend_entrypoint_t)GetProcAddress(own_module,
                                                    "g_direct3d9_entrypoint");
            if (d3d9_entrypoint) {
                frontend_entrypoint* d3d9_impl = d3d9_entrypoint();
                frontend_modules.push_back(
                  std::make_shared<frontend_module>(static_cast<HMODULE>(nullptr), d3d9_impl));
            }
        }

        {
            auto ep = g_direct2d_entrypoint;
            if (ep) {
                frontend_entrypoint* d2d_impl = ep();
                frontend_modules.push_back(
                  std::make_shared<frontend_module>(static_cast<HMODULE>(nullptr), d2d_impl));
            }
        }
        modules_loaded = true;
    });
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

static service_factory_single_t<wave::frontend_module_init_stage>
  g_frontend_module_init_stage;
