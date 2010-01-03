#include "PchSeekbar.h"
#include "CacheImpl.h"

static service_factory_single_t<wave::cache_impl> g_asdf;
static initquit_factory_t<wave::cache_initquit> g_sadf;

//DECLARE_COMPONENT_VERSION("Waveform cache", "0.0.36", "Zao")