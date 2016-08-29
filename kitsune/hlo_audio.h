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

typedef enum {
	mono_from_quad_by_channel,
	quad_decision_bits_from_quad,
	mono_from_mono

} EAudioReadType_t;


void hlo_audio_init(EAudioReadType_t read_type);

void hlo_audio_set_read_type(EAudioReadType_t read_type); //change read-type mid-stream
void hlo_audio_set_channel(uint32_t channel);


hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint32_t direction);

hlo_stream_t * hlo_audio_open_mono(uint32_t sr, uint32_t direction);

hlo_stream_t * hlo_light_stream( hlo_stream_t * base);

hlo_stream_t * hlo_stream_en( hlo_stream_t * base );

typedef enum {
	DOWNSAMPLE,
	UPSAMPLE
} sr_snv_dir;
hlo_stream_t * hlo_stream_sr_cnv( hlo_stream_t * base, sr_snv_dir dir );

hlo_stream_t * hlo_stream_tunes();
#endif
