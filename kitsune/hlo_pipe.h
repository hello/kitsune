/*
 * Interface for pipe that connect streams
 */

#ifndef HLO_PIPE_H
#define HLO_PIPE_H
#include <stddef.h>
#include <stdint.h>
#include "hlo_stream.h"

typedef struct{
	hlo_stream_t * from;
	hlo_stream_t * to;
	uint32_t poll_delay;
	uint32_t buf_size;
}hlo_pipe_t;

hlo_pipe_t * hlo_pipe_new(hlo_stream_t * from, hlo_stream_t * to, uint32_t buf_size, uint32_t opt_poll_delay);

int hlo_pipe_run(hlo_pipe_t * pipe);

void hlo_pipe_destroy(hlo_pipe_t * pipe);

#endif
