#ifndef _KEYWORD_NET_H_
#define _KEYWORD_NET_H_

#include <stdint.h>

typedef enum {
	none = 0,
	okay_sense,
	okay,
//	snooze,
//	alexa,
	NUM_KEYWORDS

} Keyword_t;

typedef void (*KeywordCallback_t)(void * context, Keyword_t keyword, int8_t value);

void keyword_net_initialize(void);

void keyword_net_register_callback(void * target_context, Keyword_t keyword, int8_t threshold,KeywordCallback_t on_start, KeywordCallback_t on_end);

void keyword_net_deinitialize(void);

void keyword_net_add_audio_samples(const int16_t * samples, uint32_t nsamples);

int cmd_test_neural_net(int argc, char * argv[]);

#endif //_KEYWORD_NET_H_
