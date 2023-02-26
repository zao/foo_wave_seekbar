project(foo_wave_seekbar)
cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED COMPONENTS serialization)
find_package(directxtk CONFIG REQUIRED)
find_package(effects11 CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(liblzma CONFIG REQUIRED)
find_package(unofficial-columns-ui-sdk CONFIG REQUIRED)
find_package(unofficial-foobar2000-sdk CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_path(WTL_INCLUDE_DIRS "atlapp.h")
find_package(ZLIB REQUIRED)

add_compile_options(-MP)

add_compile_definitions(UNICODE _UNICODE)

set(SOURCES
  foo_wave_seekbar/BackingStore.cc
  foo_wave_seekbar/CacheImpl.cc
  foo_wave_seekbar/CacheImpl.ProcessFile.cc
  foo_wave_seekbar/Clipboard.cc
  foo_wave_seekbar/FrontendLoader.cc
  foo_wave_seekbar/GdiFallback.cc
  foo_wave_seekbar/MainCache.cc
  foo_wave_seekbar/MainSeekbar.cc
  foo_wave_seekbar/MenuCommands.cc
  foo_wave_seekbar/Pack.cc
  foo_wave_seekbar/PchSeekbar.cc
  foo_wave_seekbar/PersistentSettings.cc
  foo_wave_seekbar/Player.cc
  foo_wave_seekbar/ProcessingContext.cc
  foo_wave_seekbar/SeekbarCui.cc
  foo_wave_seekbar/SeekbarDui.cc
  foo_wave_seekbar/SeekbarWindow.Callbacks.cc
  foo_wave_seekbar/SeekbarWindow.cc
  foo_wave_seekbar/SeekbarWindow.ConfigDialog.cc
  foo_wave_seekbar/SeekbarWindow.Events.cc
  foo_wave_seekbar/BackingStore.h
  foo_wave_seekbar/Cache.h
  foo_wave_seekbar/CacheImpl.h
  foo_wave_seekbar/Clipboard.h
  foo_wave_seekbar/FrontendCallbackImpl.h
  foo_wave_seekbar/FrontendConfigImpl.h
  foo_wave_seekbar/FrontendLoader.h
  foo_wave_seekbar/GdiFallback.h
  foo_wave_seekbar/Helpers.h
  foo_wave_seekbar/Job.h
  foo_wave_seekbar/Pack.h
  foo_wave_seekbar/PchSeekbar.h
  foo_wave_seekbar/PersistentSettings.h
  foo_wave_seekbar/Player.h
  foo_wave_seekbar/ProcessingContext.h
  foo_wave_seekbar/resource.h
  foo_wave_seekbar/SeekbarCui.h
  foo_wave_seekbar/SeekbarDui.h
  foo_wave_seekbar/SeekbarWindow.h
  foo_wave_seekbar/SeekCallback.h
  foo_wave_seekbar/SeekTooltip.h
  foo_wave_seekbar/foo_wave_seekbar.rc
  foo_wave_seekbar/frontend_direct2d/Direct2D1.cc
  foo_wave_seekbar/frontend_direct2d/EntrypointD2D.cc
  foo_wave_seekbar/frontend_direct2d/Direct2D.h
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.cc
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.ConfigDialog.cc
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.Effects.cc
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.Resources.cc
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.Update.cc
  foo_wave_seekbar/frontend_direct3d11/EntrypointD3D11.cc
  foo_wave_seekbar/frontend_direct3d11/PchDirect3D11.cc
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.Effects.h
  foo_wave_seekbar/frontend_direct3d11/Direct3D11.h
  foo_wave_seekbar/frontend_direct3d11/PchDirect3D11.h
  foo_wave_seekbar/frontend_sdk/FrontendHelpers.h
  foo_wave_seekbar/frontend_sdk/VisualFrontend.h
  foo_wave_seekbar/util/Asio.h
  foo_wave_seekbar/util/Barrier.h
  foo_wave_seekbar/util/Filesystem.h
  foo_wave_seekbar/waveform_sdk/Waveform.cc
  foo_wave_seekbar/waveform_sdk/WaveformImpl.cc
  foo_wave_seekbar/waveform_sdk/Downmix.h
  foo_wave_seekbar/waveform_sdk/Optional.h
  foo_wave_seekbar/waveform_sdk/RefPointer.h
  foo_wave_seekbar/waveform_sdk/Waveform.h
  foo_wave_seekbar/waveform_sdk/WaveformImpl.h
)

add_library(foo_wave_seekbar SHARED
	${SOURCES}
)

target_include_directories(foo_wave_seekbar
	PRIVATE
	${WTL_INCLUDE_DIRS}
)

target_link_libraries(foo_wave_seekbar
  PRIVATE
  "d3d11.lib"
  "d3dcompiler.lib"
  "shlwapi.lib"
  Boost::boost
	Boost::serialization
  jsoncpp_lib
	jsoncpp_object
	JsonCpp::JsonCpp
	liblzma::liblzma
  Microsoft::DirectXTK
  Microsoft::Effects11
	unofficial::columns-ui-sdk::columns_ui_sdk
    unofficial::foobar2000-sdk::foobar2000_component_client
    unofficial::foobar2000-sdk::helpers
    unofficial::foobar2000-sdk::libPPUI
    unofficial::foobar2000-sdk::pfc
    unofficial::foobar2000-sdk::SDK
    unofficial::foobar2000-sdk::shared
	unofficial::sqlite3::sqlite3
	ZLIB::ZLIB
)

add_custom_command(TARGET foo_wave_seekbar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/SciLexer.dll"
        $<TARGET_FILE_DIR:foo_wave_seekbar>)