set(WAVE_SDK_SOURCES
	"Downmix.h"
	"Optional.h"
	"RefPointer.h"
	"Waveform.cc"
	"Waveform.h"
	"WaveformImpl.cc"
	"WaveformImpl.h"
)
add_library(waveform_sdk STATIC
	${WAVE_SDK_SOURCES}
)