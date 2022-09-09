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
set(UTIL_SOURCES
	"util/Filesystem.h"
)

source_group(seekbar FILES ${SEEKBAR_SOURCES})
source_group(cache FILES ${CACHE_SOURCES})
source_group(resource FILES ${RESOURCE_SOURCES})
source_group(contrib\\sqlite3 FILES ${SQLITE_SOURCES})
source_group(contrib\\lzma FILES ${LZMA_SOURCES})
source_group(contrib\\zlib FILES ${ZLIB_SOURCES})
source_group(contrib\\json FILES ${JSON_SOURCES})
source_group(util FILES ${UTIL_SOURCES})

add_library(foo_wave_seekbar SHARED
	${RESOURCE_SOURCES}
	${CACHE_SOURCES}
	${SEEKBAR_SOURCES}
	${SQLITE_SOURCES}
	${LZMA_SOURCES}
	${ZLIB_SOURCES}
	${JSON_SOURCES}
	${UTIL_SOURCES}
)

target_compile_definitions(foo_wave_seekbar
	PRIVATE
	NOMINMAX _SCL_SECURE_NO_WARNINGS
)
target_compile_features(foo_wave_seekbar
	PRIVATE
	cxx_std_20
)
target_include_directories(foo_wave_seekbar
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	"${WTL_INCLUDE_DIRS}"
)
# target_link_directories(foo_wave_seekbar
# 	PRIVATE
# 	"${DXSDK_LIBRARY_DIRS}"
# )

target_link_libraries(foo_wave_seekbar
	PRIVATE
	waveform_sdk
	"comdlg32.lib"
	"rpcrt4.lib"
	"winmm.lib"
	"ws2_32.lib"
	"psapi.lib"
	"iphlpapi.lib"
	7zip::7zip
	Boost::boost
	unofficial::columns-ui-sdk::columns_ui_sdk
	unofficial::foobar2000-sdk::foobar2000_component_client
	unofficial::foobar2000-sdk::helpers
	unofficial::foobar2000-sdk::pfc
	unofficial::foobar2000-sdk::SDK
	unofficial::foobar2000-sdk::shared
	unofficial::sqlite3::sqlite3
	ZLIB::ZLIB
)

add_subdirectory(frontend_direct2d)
# add_subdirectory(frontend_direct3d9)
