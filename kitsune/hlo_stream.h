/*
 * Interface for stream io between drivers
 */

#ifndef HLO_STREAM_H
#define HLO_STREAM_H
#include <stddef.h>

//meta info for the parent api
typedef struct hlo_stream_info_t{

}hlo_stream_info_t;


typedef struct{
	int (*write)(void * ctx, const void * buf, size_t size);
	int (*read)(void * ctx, void * buf, size_t size);
	int (*close)(void * ctx);
}hlo_stream_vftbl;

typedef struct{
	hlo_stream_vftbl impl;
	hlo_stream_info_t info;
	void * ctx;
}hlo_stream_t;

//nonblocking pair
int hlo_stream_write(hlo_stream_t * stream, void * buf, size_t size);
int hlo_stream_read(hlo_stream_t * stream, void * buf, size_t size);

//blocking pair
//int hlo_stream_write_blocking(hlo_stream_t * stream, void * buf, size_t size);
//int hlo_stream_read_blocking(hlo_stream_t * stream, void * buf, size_t size);

int hlo_stream_close(hlo_stream_t * stream);

//implementation specific, do not call this directly
hlo_stream_t * hlo_stream_new(const hlo_stream_vftbl * impl, void * ctx);


/**
 * example in-memory implementation
 */
hlo_stream_t * fifo_stream_open(size_t capacity);

#endif
