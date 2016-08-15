#ifndef HLO_AUDIO
#define HLO_AUDIO
#include "hlo_stream.h"
#include "stdbool.h"

/**
 * driver for audio
 * do not access the stream directly, use hlo_audio_manager instead.
 */
#define HLO_AUDIO_PLAYBACK 	HLO_STREAM_WRITE
#define HLO_AUDIO_RECORD	HLO_STREAM_READ

void hlo_audio_init(void);
hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint8_t vol, uint32_t direction);

hlo_stream_t * hlo_light_stream( hlo_stream_t * base);

hlo_stream_t * hlo_stream_en( hlo_stream_t * base );

typedef enum {
	DOWNSAMPLE,
	UPSAMPLE
} sr_snv_dir;
hlo_stream_t * hlo_stream_sr_cnv( hlo_stream_t * base, sr_snv_dir dir );

hlo_stream_t * hlo_stream_tunes();
#endif
