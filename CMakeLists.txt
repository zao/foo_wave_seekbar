include_directories(${WTL_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${TBB_INCLUDE_DIRS})
include_directories(AFTER ${DXSDK_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
link_directories(${TBB_LIBRARY_DIRS})
link_directories(${DXSDK_LIBRARY_DIRS})

add_subdirectory(waveform_sdk)
add_subdirectory(frontend_sdk)

set(SEEKBAR_SOURCES
	"BackingStore.cc"
	"CacheImpl.cc"
	"CacheImpl.ProcessFile.cc"
	"Clipboard.cc"
	"GdiFallback.cc"
	"MainCache.cc"
	"MainSeekbar.cc"
	"MenuCommands.cc"
	"Pack.cc"
	"PersistentSettings.cc"
	"ProcessingContext.cc"
	"SeekbarCui.cc"
	"SeekbarDui.cc"
	"SeekbarWindow.Callbacks.cc"
	"SeekbarWindow.cc"
	"SeekbarWindow.ConfigDialog.cc"
	"SeekbarWindow.Events.cc"
	"utf8_codecvt_facet.cpp"
	"BackingStore.h"
	"Cache.h"
	"CacheImpl.h"
	"Clipboard.h"
	"FrontendCallbackImpl.h"
	"FrontendConfigImpl.h"
	"GdiFallback.h"
	"Helpers.h"
	"InstanceTracker.h"
	"Job.h"
	"Pack.h"
	"PchSeekbar.h"
	"PersistentSettings.h"
	"PfcSpirit.h"
	"ProcessingContext.h"
	"resource.h"
	"SeekbarCui.h"
	"SeekbarDui.h"
	"SeekbarState.h"
	"SeekbarWindow.h"
	"SeekCallback.h"
	"SeekTooltip.h"
	"foo_wave_seekbar.rc"
)
set(SQLITE_SOURCES
	"sqlite3.c"
	"sqlite3.h"
)
set(LZMA_SOURCES
	"lzma/LzFind.c"
	"lzma/LzFindMt.c"
	"lzma/Lzma2Dec.c"
	"lzma/Lzma2Enc.c"
	"lzma/LzmaDec.c"
	"lzma/LzmaEnc.c"
	"lzma/MtCoder.c"
	"lzma/Threads.c"
)
set(ZLIB_SOURCES
	"zlib/adler32.c"
	"zlib/compress.c"
	"zlib/crc32.c"
	"zlib/deflate.c"
	"zlib/infback.c"
	"zlib/inffast.c"
	"zlib/inflate.c"
	"zlib/inftrees.c"
	"zlib/trees.c"
	"zlib/uncompr.c"
	"zlib/zutil.c"
)

set(UTIL_SOURCES
	"util/Asio.h"
)

set(TESTS_SOURCES
	"tests/TestProcessFile.cc"
)

source_group(contrib\\sqlite3 FILES ${SQLITE_SOURCES})
source_group(contrib\\lzma FILES ${LZMA_SOURCES})
source_group(contrib\\zlib FILES ${ZLIB_SOURCES})
source_group(util FILES ${UTIL_SOURCES})
source_group(tests FILES ${TESTS_SOURCES})

include_directories(.)
add_definitions(-Zm300)
enable_precompiled_headers("PchSeekbar.h" SEEKBAR_SOURCES)
add_library(foo_wave_seekbar SHARED
	${SEEKBAR_SOURCES}
	${SQLITE_SOURCES}
	${LZMA_SOURCES}
	${ZLIB_SOURCES}
	${UTIL_SOURCES}
	${TESTS_SOURCES}
)

target_link_libraries(foo_wave_seekbar
	columns_ui-sdk
	foobar2000_component_client
	helpers
	pfc
	SDK
	shared
	waveform_sdk
	"comdlg32.lib"
	"rpcrt4.lib"
	"winmm.lib"
)
deploy_component(foo_wave_seekbar foo_wave_seekbar)

add_subdirectory(frontend_direct2d)
add_subdirectory(frontend_direct3d9)