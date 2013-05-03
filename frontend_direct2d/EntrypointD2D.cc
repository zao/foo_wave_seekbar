//          Copyright Lars Viklund 2008 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Direct2D.h"

#if defined(BOOST_ALL_NO_LIB)
#  if defined(_DEBUG)
#    pragma comment(lib, "libboost_filesystem-mt-gd.lib")
#    pragma comment(lib, "libboost_system-mt-gd.lib")
#    pragma comment(lib, "libboost_thread-mt-gd.lib")
#  else
#    pragma comment(lib, "libboost_filesystem-mt.lib")
#    pragma comment(lib, "libboost_system-mt.lib")
#    pragma comment(lib, "libboost_thread-mt.lib")
#  endif
#endif

FOO_WAVE_SEEKBAR_VISUAL_FRONTEND_ENTRYPOINT(wave::config::frontend_direct2d1, wave::direct2d1_frontend)