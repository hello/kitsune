/*
 * Pipes are an extension of the streams API
 * that provide blocking behavior for streams
 */

#ifndef HLO_PIPE_H
#define HLO_PIPE_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "hlo_stream.h"

#include "FreeRTOS.h"
#include "semphr.h"


/**
 * filters are like pipes except it also processes data before dumping it out to output.
 */
typedef uint8_t (*hlo_stream_signal)(void * ctx);
typedef int(*hlo_filter)(hlo_stream_t * input, hlo_stream_t * opt_output, void * ctx, hlo_stream_signal signal);
#define BREAK_ON_SIG(s) if(s && s(ctx)){break;}

#include "streaming.pb.h"
typedef struct {
	hlo_stream_t * source;
	hlo_stream_t * sink;
	bool flush;
	int state;
	void * ctx;
	uint64_t id;
	xSemaphoreHandle join_sem;
	Preamble_pb_type hlo_pb_type;
} pipe_ctx;

int frame_pipe_encode( pipe_ctx * pipe );
int frame_pipe_decode( pipe_ctx * pipe );
void thread_frame_pipe_encode(void* ctx);
void thread_frame_pipe_decode(void* ctx);

#endif
