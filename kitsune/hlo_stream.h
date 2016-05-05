/*
 * Interface for stream io between drivers
 */

#ifndef HLO_STREAM_H
#define HLO_STREAM_H
#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

//options
#define HLO_STREAM_WRITE		1
#define HLO_STREAM_READ	 		2
#define HLO_STREAM_READ_WRITE	(HLO_STREAM_WRITE + HLO_STREAM_READ)

//steam return codes
#define HLO_STREAM_ERROR		-1
#define HLO_STREAM_EOF			-2
#define HLO_STREAM_EAGAIN		-11
#define HLO_STREAM_NULL_OBJ		-20
#define HLO_STREAM_NO_IMPL		-21



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
int hlo_stream_write(hlo_stream_t * stream, const void * buf, size_t size);
int hlo_stream_read(hlo_stream_t * stream, void * buf, size_t size);

/**
 * closes the stream, the stream object is considered invalid after this call
 * IMPORTANT: Any of the following condition, when met, does not deallocate the wrapper object's memory 
 * 1.  the close method is not implemented (Usually means it's a singleton stream that lives on the static memory)
 * 2.  the close method is implemented, but returns an error (Usually means it has deinit errors)
 */
int hlo_stream_close(hlo_stream_t * stream);

//Initializes a stream memory region.
//do not use in conjunction with new.
//use this to initialize statically allocated streams.
void hlo_stream_init(hlo_stream_t * stream,
		const hlo_stream_vftbl_t * impl,
		void * ctx,
		uint32_t options);

//Allocates and initializes a stream.
//do not use in conjunction with init.
hlo_stream_t * hlo_stream_new(const hlo_stream_vftbl_t * impl, void * ctx, uint32_t options);

int Cmd_make_stream(int argc, char *argv[]);
int Cmd_write_stream(int argc, char *argv[]);
int Cmd_read_stream(int argc, char *argv[]);

//fifo buffer stream
hlo_stream_t * fifo_stream_open(size_t capacity);
//random stream
hlo_stream_t * random_stream_open(void);
//debug stream
hlo_stream_t * debug_stream_open(void);

#endif
