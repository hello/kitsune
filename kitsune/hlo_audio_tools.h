#ifndef HLO_AUDIO_TOOLS_H
#define HLO_AUDIO_TOOLS_H
#include "audiotask.h"
#include "hlo_pipe.h"
/**
 * contains a list of commands and filters for audio
 */
typedef struct{
	uint8_t raw_audio_upload_enable;
}feature_extractor_ctx_t;
int hlo_filter_feature_extractor(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_adpcm_decoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_adpcm_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
/**
 * filter for feature extraction
 */

//an audio recorder app
//that demonstrates how to record and playback audio
void hlo_audio_recorder_task(hlo_stream_t * data);
void hlo_audio_playback_task(AudioPlaybackDesc_t * desc, hlo_stream_signal sig_stop);
void hlo_audio_octogram_task(void * data);

//uart commands
int Cmd_audio_record_start(int argc, char *argv[]);
int Cmd_audio_record_stop(int argc, char *argv[]);
int Cmd_audio_record_replay(int argc, char *argv[]);
int Cmd_audio_octogram(int argc, char *argv[]);
int Cmd_audio_features(int argc, char *argv[]);
#endif
