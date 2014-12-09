include_directories("${WTL_INCLUDE_DIRS}")
include_directories("${ASIO_ROOT}/include")
include_directories("${Boost_INCLUDE_DIRS}")
include_directories("${TBB_INCLUDE_DIRS}")
include_directories("${LIBUV_INCLUDE_DIRS}")
include_directories(AFTER "${DXSDK_INCLUDE_DIRS}")
link_directories("${Boost_LIBRARY_DIRS}")
link_directories("${TBB_LIBRARY_DIRS}")
link_directories("${LIBUV_LIBRARY_DIRS}")
link_directories("${DXSDK_LIBRARY_DIRS}")

include_directories(${LIBLFDS_INCLUDE_DIR})
link_directories(${LIBLFDS_LIBRARIES})

add_subdirectory(waveform_sdk)
add_subdirectory(frontend_sdk)

set(CACHE_SOURCES
	"BackingStore.cc"
	"BackingStore.h"
	"Cache.h"
	"CacheImpl.cc"
	"CacheImpl.h"
	"CacheImpl.ProcessFile.cc"
	"Job.h"
	"MainCache.cc"
	"MenuCommands.cc"
	"Pack.cc"
	"Pack.h"
	"ProcessingContext.cc"
	"ProcessingContext.h"
)
set(SEEKBAR_SOURCES
	"Clipboard.cc"
	"Clipboard.h"
	"FrontendLoader.cc"
	"FrontendLoader.h"
	"FrontendCallbackImpl.h"
	"FrontendConfigImpl.h"
	"GdiFallback.cc"
	"GdiFallback.h"
	"Helpers.h"
	"MainSeekbar.cc"
	"PchSeekbar.h"
	"PersistentSettings.cc"
	"PersistentSettings.h"
	"SeekbarCui.cc"
	"SeekbarCui.h"
	"SeekbarDui.cc"
	"SeekbarDui.h"
	"SeekbarWindow.Callbacks.cc"
	"SeekbarWindow.cc"
	"SeekbarWindow.ConfigDialog.cc"
	"SeekbarWindow.Events.cc"
	"SeekbarWindow.h"
	"SeekCallback.h"
	"SeekTooltip.h"
	"Player.cc"
	"Player.h"
)
set(RESOURCE_SOURCES
	"foo_wave_seekbar.rc"
	"resource.h"
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
set(JSON_SOURCES
	"json/json.h"
	"json/jsoncpp.cpp"
	"json/json-forwards.h"
)
set(UTIL_SOURCES
	"util/Filesystem.h"
)
set(TESTS_SOURCES
	"tests/TestProcessFile.cc"
)

source_group(seekbar FILES ${SEEKBAR_SOURCES})
source_group(cache FILES ${CACHE_SOURCES})
source_group(resource FILES ${RESOURCE_SOURCES})
source_group(contrib\\sqlite3 FILES ${SQLITE_SOURCES})
source_group(contrib\\lzma FILES ${LZMA_SOURCES})
source_group(contrib\\zlib FILES ${ZLIB_SOURCES})
source_group(contrib\\json FILES ${JSON_SOURCES})
source_group(util FILES ${UTIL_SOURCES})
source_group(tests FILES ${TESTS_SOURCES})

include_directories(.)
add_definitions(-Zm300 -DNOMINMAX)
enable_precompiled_headers("PchSeekbar.h" SEEKBAR_SOURCES)
add_library(foo_wave_seekbar SHARED
	${RESOURCE_SOURCES}
	${CACHE_SOURCES}
	${SEEKBAR_SOURCES}
	${SQLITE_SOURCES}
	${LZMA_SOURCES}
	${ZLIB_SOURCES}
	${JSON_SOURCES}
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
	"libuv.lib"
	"ws2_32.lib"
	"psapi.lib"
	"iphlpapi.lib"
)
deploy_component(foo_wave_seekbar foo_wave_seekbar)

add_subdirectory(frontend_direct2d)
add_subdirectory(frontend_direct3d9)
