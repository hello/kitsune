#ifndef _KEYWORD_NET_H_
#define _KEYWORD_NET_H_



#include <stdint.h>
#include "tinytensor_features.h" //this is a hack.  put definitions in another header
#include "net_stats.h"
#include "keywords.h"

typedef void (*KeywordCallback_t)(void * context, Keyword_t keyword, int16_t value);

void keyword_net_initialize(void);

void keyword_net_register_callback(void * target_context, Keyword_t keyword, int16_t threshold,uint32_t min_duration,KeywordCallback_t on_start, KeywordCallback_t on_end);

void keyword_net_register_speech_callback(void * context, tinytensor_speech_detector_callback_t callback);

void keyword_net_deinitialize(void);

void keyword_net_add_audio_samples(const int16_t * samples, uint32_t nsamples);

void keyword_net_resume_net_operation(void);

void keyword_net_pause_net_operation(void);

void keyword_net_reset_states(void);

uint8_t keyword_net_get_and_reset_stats(NetStats_t * stats);

int cmd_set_debug_streaming(int argc, char * argv[]);

int cmd_test_neural_net(int argc, char * argv[]);

#endif //_KEYWORD_NET_H_
