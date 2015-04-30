#ifndef HLO_AUDIO
#define HLO_AUDIO
#include "hlo_stream.h"


#define HLO_AUDIO_PLAYBACK 	HLO_STREAM_WRITE
#define HLO_AUDIO_RECORD	HLO_STREAM_READ

hlo_stream_t * hlo_audio_open(uint32_t sr, uint32_t direction);

#endif
