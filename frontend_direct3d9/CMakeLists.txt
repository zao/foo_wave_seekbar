set(D3D9_SOURCES
    "Direct3D9.cc"
    "Direct3D9.ConfigDialog.cc"
    "Direct3D9.Effects.cc"
    "Direct3D9.Resources.cc"
    "Direct3D9.Update.cc"
    "EntrypointD3D9.cc"
    "PchDirect3D9.cc"
    "Direct3D9.Effects.h"
    "Direct3D9.h"
    "PchDirect3D9.h"
    "resource.h"
    "Scintilla.h"
    "frontend_direct3d9.rc"
)

include_directories(${MICROHTTPD_INCLUDE_DIRS})

add_definitions(-Zm300)
enable_precompiled_headers("PchDirect3D9.h" D3D9_SOURCES)

add_library(frontend_direct3d9 SHARED
    ${D3D9_SOURCES}
)

# set_target_properties(frontend_direct3d9 PROPERTIES LINK_FLAGS "/DELAYLOAD:d3dx9_42.dll")

target_link_libraries(frontend_direct3d9
    pfc
    waveform_sdk
    "d3d9.lib"
    "d3dx9.lib"
    ${MICROHTTPD_LIBRARIES}
)
deploy_component(frontend_direct3d9 foo_wave_seekbar)
deploy_aux_directory(frontend_direct3d9 foo_wave_seekbar/editor "${CMAKE_CURRENT_SOURCE_DIR}/editor")
