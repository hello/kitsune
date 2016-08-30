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
 * blocking transfers of @buf_size bytes into/from the stream unless an error has occured.
 * retries at @transfer_delay interval
 */
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

/**
 * same as @hlo_stream_transfer_all but tranfsers remainging bytes and returns when
 *  @flush is true
 */
int hlo_stream_transfer_until(transfer_direction direction,
							hlo_stream_t * stream,
							uint8_t * buf,
							uint32_t buf_size,
							uint32_t transfer_delay,
							bool * flush );

/**
 * blocking transfers of @buf_size bytes between the @src and @dst streams
 * retries at @transfer_delay interval
 */
int hlo_stream_transfer_between(
		hlo_stream_t * src,
		hlo_stream_t * dst,
		uint8_t * buf,
		uint32_t buf_size,
		uint32_t transfer_delay);

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
