#include "hlo_stream.h"
#include <assert.h>
#include <strings.h>
#include "FreeRTOS.h"




int hlo_stream_write(hlo_stream_t * stream, void * buf, size_t size){
	assert(stream);
	assert(stream->impl.write);

	return stream->impl.write(stream->ctx, buf, size);
}

int hlo_stream_read(hlo_stream_t * stream, void * buf, size_t size){
	assert(stream);
	assert(stream->impl.read);

	return stream->impl.read(stream->ctx, buf, size);
}

int hlo_stream_close(hlo_stream_t * stream){
	int ret = 0;

	assert(stream);
	assert(stream->impl.close);

	ret = stream->impl.close(stream->ctx);
	if(ret == 0){
		vPortFree(stream);
	}
	return ret;
}

hlo_stream_t * hlo_stream_new(const hlo_stream_vftbl_t * impl, void * ctx){
	hlo_stream_t * ret  = pvPortMalloc(sizeof(hlo_stream_t));

	assert(ret);

	memset(ret, 0, sizeof(*ret));
	ret->ctx = ctx;
	ret->impl = *impl;

	return ret;
}


////==========================================================
//example fifo stream below, no thread safety, no optimization
typedef struct{
	volatile int write_idx;
	volatile int read_idx;
	volatile int filled;
	int capacity;
	uint8_t buf[0];
}fifo_stream_t;

static int fifo_read_byte(fifo_stream_t * ctx, uint8_t * buf){
	*buf = ctx->buf[ctx->read_idx];
	ctx->read_idx = (ctx->read_idx + 1)%ctx->capacity;
	ctx->filled--;
	return ctx->filled;
}

static int fifo_write_byte(fifo_stream_t * ctx, uint8_t * buf){
	ctx->buf[ctx->write_idx] = *buf;
	ctx->write_idx = (ctx->write_idx + 1)%ctx->capacity;
	ctx->filled++;
	return ctx->filled;
}

static int fifo_write(void * ctx, const void * buf, size_t size){
	fifo_stream_t * fifo = (fifo_stream_t*)ctx;
	if(fifo->filled < fifo->capacity){
		int written = 0;
		while(size && fifo->filled < fifo->capacity){
			fifo_write_byte(fifo, (uint8_t*)buf+written);
			written++;
			size--;
		}
		return written;
	}else{
		return EAGAIN;
	}
}
static int fifo_read(void * ctx, void * buf, size_t size){
	fifo_stream_t * fifo = (fifo_stream_t*)ctx;
	if(fifo->filled){
		int read = 0;
		while(read < size && fifo->filled){
			fifo_read_byte(fifo, (uint8_t*)buf+read);
			read++;
		}
		return read;
	}else{
		return EAGAIN;
	}

}
static int fifo_close(void * ctx){
	vPortFree(ctx);
	return 0;
}

static hlo_stream_vftbl_t fifo_stream_impl = {
		.write = fifo_write,
		.read = fifo_read,
		.close = fifo_close
};

hlo_stream_t * fifo_stream_open(size_t capacity){
	size_t size = sizeof(fifo_stream_t) + capacity;
	fifo_stream_t * fifo = pvPortMalloc(size);

	if(fifo){
		memset(fifo, 0, size);
		fifo->capacity = capacity;
		return hlo_stream_new(&fifo_stream_impl, fifo);
	}else{
		return NULL;
	}

}

////==========================================================
//test commands
#include "uart_logger.h"
static hlo_stream_t *user_streams[3];

int Cmd_make_stream(int argc, char *argv[]){
	DISP("Test: Making fifo 0");
	user_streams[0] = fifo_stream_open(16);
	return 0;
}
int Cmd_write_stream(int argc, char *argv[]){
	DISP("Test: Writing fifo 0");
	if(argc > 1){
		int ret = hlo_stream_write(user_streams[0], argv[1], strlen(argv[1]));
		DISP("write %s, return code %d\r\n",argv[1], ret);
	}
	return 0;
}
int Cmd_read_stream(int argc, char *argv[]){
	if(argc > 0){
		uint8_t temp[9] = {0};
		int ret = hlo_stream_read(user_streams[0], temp, 8);
		DISP("read %s, return code %d\r\n", temp, ret);
	}
	return 0;
}
