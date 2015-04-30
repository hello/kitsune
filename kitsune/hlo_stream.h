/*
 * Interface for stream io between drivers
 */

#ifndef HLO_STREAM_H
#define HLO_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include "socket.h"
#include "FreeRTOS.h"
#include "semphr.h"

//options
#define HLO_STREAM_IN	 		1
#define HLO_STREAM_OUT	 		2
#define HLO_STREAM_IN_OUT		(HLO_STREAM_IN + HLO_STREAM_OUT)
#define HLO_STREAM_BLOCKING 	4

//meta info for the parent api
typedef struct hlo_stream_info_t{
	uint32_t bytes_written;
	uint32_t bytes_read;
	uint32_t options;
	xSemaphoreHandle lock;
}hlo_stream_info_t;


typedef struct{
	int (*write)(void * ctx, const void * buf, size_t size);
	int (*read)(void * ctx, void * buf, size_t size);
	int (*close)(void * ctx);
}hlo_stream_vftbl_t;

typedef struct{
	hlo_stream_vftbl_t impl;
	hlo_stream_info_t info;
	void * ctx;
}hlo_stream_t;

//view info
void hlo_stream_info(const hlo_stream_t * stream);

//nonblocking pair
int hlo_stream_write(hlo_stream_t * stream, void * buf, size_t size);
int hlo_stream_read(hlo_stream_t * stream, void * buf, size_t size);


//blocking pair
//int hlo_stream_write_blocking(hlo_stream_t * stream, void * buf, size_t size);
//int hlo_stream_read_blocking(hlo_stream_t * stream, void * buf, size_t size);

int hlo_stream_close(hlo_stream_t * stream);

//implementation specific, do not call this directly
hlo_stream_t * hlo_stream_new(const hlo_stream_vftbl_t * impl, void * ctx, uint32_t options);

int Cmd_make_stream(int argc, char *argv[]);
int Cmd_write_stream(int argc, char *argv[]);
int Cmd_read_stream(int argc, char *argv[]);
/**
 * example in-memory implementation
 */
hlo_stream_t * fifo_stream_open(size_t capacity);

#endif
