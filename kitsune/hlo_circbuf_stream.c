#include "hlo_circbuf_stream.h"
#include <string.h>

static inline int circ_read_byte(hlo_circbuf_stream_t * ctx, uint8_t * buf){
	*buf = ctx->buf[ctx->read_idx];
	ctx->read_idx = (ctx->read_idx + 1)%ctx->capacity;
	ctx->filled--;
	return ctx->filled;
}

static inline int circ_write_byte(hlo_circbuf_stream_t * ctx, uint8_t * buf){
	ctx->buf[ctx->write_idx] = *buf;
	ctx->write_idx = (ctx->write_idx + 1)%ctx->capacity;
	ctx->filled++;
	return ctx->filled;
}

static int circ_write(void * ctx, const void * buf, size_t size){
	hlo_circbuf_stream_t * circ = (hlo_circbuf_stream_t*)ctx;
	uint8_t dummy;

	while(circ->filled > circ->capacity - size - 1){
		circ_read_byte(circ, &dummy);
	}
	int written = 0;
	while(size && circ->filled < circ->capacity){
		circ_write_byte(circ, (uint8_t*)buf+written);
		written++;
		size--;
	}
	return written;
}
static int circ_read(void * ctx, void * buf, size_t size){
	hlo_circbuf_stream_t * circ = (hlo_circbuf_stream_t*)ctx;
	if(circ->filled){
		int read = 0;
		while(read < size && circ->filled){
			circ_read_byte(circ, (uint8_t*)buf+read);
			read++;
		}
		return read;
	}else{
		return HLO_STREAM_EOF;
	}

}
static int circ_close(void * ctx){
	vPortFree(ctx);
	return 0;
}

static hlo_stream_vftbl_t circ_stream_impl = {
		.write = circ_write,
		.read = circ_read,
		.close = circ_close
};

hlo_stream_t * hlo_circbuf_stream_open(size_t capacity){
	size_t size = sizeof(hlo_circbuf_stream_t) + capacity;
	hlo_circbuf_stream_t * circ = pvPortMalloc(size + 32);

	if(circ){
		memset(circ, 0, size);
		circ->capacity = capacity;
		return hlo_stream_new(&circ_stream_impl, circ, HLO_STREAM_READ_WRITE);
	}else{
		return NULL;
	}

}
