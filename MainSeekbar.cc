//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PchSeekbar.h"
#include "SeekbarCui.h"
#include "SeekbarDui.h"

#if defined(BOOST_ALL_NO_LIB)
#  if defined(_DEBUG)
#    pragma comment(lib, "libboost_filesystem-mt-gd.lib")
#    pragma comment(lib, "libboost_regex-mt-gd.lib")
#    pragma comment(lib, "libboost_system-mt-gd.lib")
#    pragma comment(lib, "libboost_thread-mt-gd.lib")
#  else
#    pragma comment(lib, "libboost_filesystem-mt.lib")
#    pragma comment(lib, "libboost_regex-mt.lib")
#    pragma comment(lib, "libboost_system-mt.lib")
#    pragma comment(lib, "libboost_thread-mt.lib")
#  endif
#endif

static service_factory_t<ui_element_impl<wave::seekbar_dui>> g_asdf;

static uie::window_factory<wave::seekbar_uie_t<uie::type_panel>> g_sadf_panel;
static uie::window_factory<wave::seekbar_uie_t<uie::type_toolbar>> g_sadf_toolbar;

DECLARE_COMPONENT_VERSION("Waveform seekbar", "0.2.39.1", "Zao")
VALIDATE_COMPONENT_FILENAME("foo_wave_seekbar.dll")
