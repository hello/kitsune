#ifndef HLO_AUDIO_TOOLS_H
#define HLO_AUDIO_TOOLS_H
#include "audiotask.h"
#include "hlo_pipe.h"
/**
 * contains a list of commands and filters for audio
 */
int hlo_filter_feature_extractor(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_adpcm_decoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_adpcm_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_data_transfer(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_octogram(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_throughput_test(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_voice_command(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_data_transfer_limited(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_filter_modulate_led_with_sound(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
//uart commands
int Cmd_audio_record_start(int argc, char *argv[]);
int Cmd_audio_record_stop(int argc, char *argv[]);
int Cmd_stream_transfer(int argc, char * argv[]);
#endif
