#ifndef _KEYWORD_NET_H_
#define _KEYWORD_NET_H_

#include <stdint.h>

typedef enum {
	none = 0,
	okay_sense,
	stop,
	snooze,
	alexa,
	NUM_KEYWORDS

} Keyword_t;

typedef void (*KeywordCallback_t)(void * context, Keyword_t keyword, int8_t value);

void initialize_keyword_net(void);

void register_keyword_callback(void * target_context, Keyword_t keyword, int8_t threshold,KeywordCallback_t on_start, KeywordCallback_t on_end);

void deinitialize_keyword_net(void);


#endif //_KEYWORD_NET_H_
