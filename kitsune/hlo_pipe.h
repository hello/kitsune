/*
 * Pipes are an extension of the streams API
 * that provide blocking behavior for streams
 */

#ifndef HLO_PIPE_H
#define HLO_PIPE_H
#include <stddef.h>
#include <stdint.h>
#include "hlo_stream.h"

/**
 * this extends the stream to allow for true blocked read/write
 * streams that do not have this implemented can only do poll and delay
 */
hlo_stream_t * hlo_stream_enable_notification(hlo_stream_t * stream);
/**
 * signals that count bytes of data are available for read
 */
void hlo_stream_notify_read(int count);
/**
 * signals that count bytes of data are available for write
 */
void hlo_stream_notify_write(int count);


/**
 * blocking transfers of @buf_size bytes into/from the stream unless an error has occured.
 * retries at @transfer_delay interval
 * if notification is enabled on the stream, transfer delay will be automatically adjusted to reduce CPU time
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
 * blocking transfers of @buf_size bytes between the @src and @dst streams
 * retries at @transfer_delay interval
 * if notification is enabled on the stream, transfer delay will be automatically adjusted to reduce CPU time
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
typedef uint8_t (*hlo_stream_signal)(void);
typedef int(*hlo_filter)(hlo_stream_t * input, hlo_stream_t * opt_output, void * ctx, hlo_stream_signal signal);
#define BREAK_ON_SIG(s) if(s && s()){break;}
#endif
