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
)

add_library(frontend_direct3d9 STATIC
	${D3D9_SOURCES}
)

target_include_directories(frontend_direct3d9
	PRIVATE
	"${WTL_INCLUDE_DIRS}"
)
# set_target_properties(frontend_direct3d9 PROPERTIES LINK_FLAGS "/DELAYLOAD:d3dx9_42.dll")

target_link_libraries(frontend_direct3d9
	PRIVATE
	frontend_sdk
	# pfc
	waveform_sdk
	"d3d9.lib"
	"d3d11.lib"
	"dxgi.lib"
	Microsoft::DirectXTK
	Microsoft::Effects11
	unofficial::scintilla::scintilla
)