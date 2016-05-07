/*
 * Pipes are an extension of the streams API
 * that provide blocking behavior for streams
 */

#ifndef HLO_PIPE_H
#define HLO_PIPE_H
#include <stddef.h>
#include <stdint.h>
#include "hlo_stream.h"

typedef enum{
	INTO_STREAM = 0,
	FROM_STREAM,
}transfer_direction;

//transfers buf_size between buffer and stream
//polls transfer_delay ticks
int hlo_stream_transfer_all(
		transfer_direction direction,
		hlo_stream_t * target,
		uint8_t * buf,
		uint32_t buf_size,
		uint32_t transfer_delay);

//transfers buf_size between stream and stream
//polls transfer_delay ticks
int hlo_stream_transfer_between(
		hlo_stream_t * src,
		hlo_stream_t * dst,
		uint8_t * buf,
		uint32_t buf_size,
		uint32_t transfer_delay);

/**
 * filters are like pipes except it also processes data before dumping it out to output.
 */
typedef uint8_t (*hlo_stream_signal)(void);
typedef int(*hlo_filter)(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
#endif
