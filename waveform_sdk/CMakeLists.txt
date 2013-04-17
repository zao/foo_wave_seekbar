set(WAVE_SDK_SOURCES
	"Waveform.cc"
	"WaveformImpl.cc"
	"Downmix.h"
	"RefPointer.h"
	"Waveform.h"
	"WaveformImpl.h"
)
add_library(waveform_sdk STATIC
	${WAVE_SDK_SOURCES}
)