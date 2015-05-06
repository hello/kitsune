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

//helper api to transfer all as indicated by buf_size
typedef enum{
	INTO_STREAM = 0,
	FROM_STREAM,
}transfer_direction;
int hlo_stream_transfer_all(
		transfer_direction direction,
		hlo_stream_t * target,
		uint8_t * buf,
		uint32_t buf_size,
		uint32_t transfer_delay);

#endif
