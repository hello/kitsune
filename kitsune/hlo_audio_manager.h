#ifndef HLO_AUDIO_MANAGER_H
#define HLO_AUDIO_MANAGER_H
#include "hlo_stream.h"

/**
 * Manages multiplexes hello_audio_stream to service multiple client apps
 */

//set to 1 to access speaker directly, 0 for mixed mode
//TODO mixing mode doesn't work very well.
#define DIRECT_SPEAKER_ACCESS 1

void hlo_audio_manager_init(void);
void hlo_audio_manager_spkr_thread(void * data);
void hlo_audio_manager_mic_thread(void * data);
////---------------------------------
// Playback multiplexer
#define NUM_MAX_PlAYBACK_CHANNELS 2
/**
 */
hlo_stream_t * hlo_open_spkr_stream(size_t buffer_size);


////---------------------------------
// Mic multiplexer
#define NUM_MAX_MIC_CHANNELS 3
/**
 * opt_always_on
 * 1: records playback
 * 0: do not record when playing back
 */
hlo_stream_t * hlo_open_mic_stream(size_t buffer_size, uint8_t opt_always_on);


#endif
