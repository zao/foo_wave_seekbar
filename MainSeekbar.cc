#include "PchSeekbar.h"
#include "SeekbarCui.h"
#include "SeekbarDui.h"
#include "Direct3D.h"
#include "Direct2D.h"

DWORD xbgr_to_argb(COLORREF c, BYTE a)
{
	return (a << 24) | (GetRValue(c) << 16) | (GetGValue(c) << 8) | (GetBValue(c));
}

static service_factory_t<ui_element_impl<wave::seekbar_dui>> g_asdf_d3d9;

static uie::window_factory<wave::seekbar_uie> g_sadf_d3d9;


DECLARE_COMPONENT_VERSION("Waveform seekbar", "0.1.6", "Zao")
VALIDATE_COMPONENT_FILENAME("foo_wave_seekbar.dll")