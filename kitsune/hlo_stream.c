#include "hlo_stream.h"
#include "kit_assert.h"
#include <strings.h>
#include <stdbool.h>
#include "uart_logger.h"

#define LOCK(stream) xSemaphoreTakeRecursive(stream->info.lock, portMAX_DELAY)
#define UNLOCK(stream) xSemaphoreGive(stream->info.lock)

#define VERIFY_STREAM(stream) if(!stream){return HLO_STREAM_NULL_OBJ;}
#define HAS_IMPL_OR_FAIL(stream, method) if(!stream->impl.method){return HLO_STREAM_NO_IMPL;}

void hlo_stream_info(const hlo_stream_t * stream){
	if(stream){
		DISP("Stream\r\n");
		DISP("Written: %u\r\n",stream->info.bytes_written);
		DISP("Read   : %u\r\n",stream->info.bytes_read);
	}

}

int hlo_stream_write(hlo_stream_t * stream, const void * buf, size_t size){
	VERIFY_STREAM(stream);
	HAS_IMPL_OR_FAIL(stream,write);

	LOCK(stream);
	int ret = stream->impl.write(stream->ctx, buf, size);
	if(ret > 0){
		stream->info.bytes_written += ret;
	}
	UNLOCK(stream);
	return ret;
}

int hlo_stream_read(hlo_stream_t * stream, void * buf, size_t size){
	VERIFY_STREAM(stream);
	HAS_IMPL_OR_FAIL(stream,read);

	LOCK(stream);
	int ret = stream->impl.read(stream->ctx, buf, size);
	if(ret > 0){
		stream->info.bytes_read += ret;
	} else if( 0 == ret && stream->info.end ) {
		ret = HLO_STREAM_EOF;
	}
	UNLOCK(stream);
	return ret;
}

int hlo_stream_close(hlo_stream_t * stream){
	int ret = 0;
	VERIFY_STREAM(stream);
	HAS_IMPL_OR_FAIL(stream,close);

	LOCK(stream);
	ret = stream->impl.close(stream->ctx);
	UNLOCK(stream);
	if(ret >= 0){
		vSemaphoreDelete(stream->info.lock);
		vPortFree(stream);
	}else if( ret == HLO_STREAM_NO_IMPL ){
		return 0;
	}
	return ret;
}
int hlo_stream_end(hlo_stream_t * stream){
	int ret = 0;
	VERIFY_STREAM(stream);

	LOCK(stream);
	stream->info.end = true;
	UNLOCK(stream);
	return ret;
}
void hlo_stream_init(hlo_stream_t * stream,
		const hlo_stream_vftbl_t * impl,
		void * ctx,
		uint32_t options){
	memset(stream, 0, sizeof(hlo_stream_t));
	stream->ctx = ctx;
	stream->impl = *impl;
	stream->info.options = options;
	stream->info.lock = xSemaphoreCreateRecursiveMutex();
	assert(stream->info.lock);
}
hlo_stream_t * hlo_stream_new(const hlo_stream_vftbl_t * impl, void * ctx, uint32_t options){
	hlo_stream_t * ret  = pvPortMalloc(sizeof(hlo_stream_t));

	assert(ret);
	hlo_stream_init(ret, impl, ctx, options);

	return ret;
}


////==========================================================
//Fifo stream, no optimization, no thread safety
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
		return 0;
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
		return 0;
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
		return hlo_stream_new(&fifo_stream_impl, fifo, HLO_STREAM_READ_WRITE);
	}else{
		return NULL;
	}

}

////==========================================================
//circulat buffer stream, no optimization, no thread safety
typedef struct{
	volatile int write_idx;
	volatile int read_idx;
	volatile int filled;
	int capacity;
	uint8_t buf[0];
}circ_stream_t;

static int circ_read_byte(circ_stream_t * ctx, uint8_t * buf){
	*buf = ctx->buf[ctx->read_idx];
	ctx->read_idx = (ctx->read_idx + 1)%ctx->capacity;
	ctx->filled--;
	return ctx->filled;
}

static int circ_write_byte(circ_stream_t * ctx, uint8_t * buf){
	ctx->buf[ctx->write_idx] = *buf;
	ctx->write_idx = (ctx->write_idx + 1)%ctx->capacity;
	ctx->filled++;
	return ctx->filled;
}

static int circ_write(void * ctx, const void * buf, size_t size){
	circ_stream_t * circ = (circ_stream_t*)ctx;
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
	circ_stream_t * circ = (circ_stream_t*)ctx;
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

hlo_stream_t * circ_stream_open(size_t capacity){
	size_t size = sizeof(circ_stream_t) + capacity;
	circ_stream_t * circ = pvPortMalloc(size);

	if(circ){
		memset(circ, 0, size);
		circ->capacity = capacity;
		return hlo_stream_new(&circ_stream_impl, circ, HLO_STREAM_READ_WRITE);
	}else{
		return NULL;
	}

}
////==========================================================
//Random Stream
void get_random(int num_rand_bytes, uint8_t *rand_data);
static int random_read(void * ctx, void * buf, size_t size){
	get_random(size,(uint8_t*)buf);
	return size;
}
static int random_seed(void *ctx, const void * buf, size_t size){
	//TODO seed rng
	return size;
}
static hlo_stream_vftbl_t random_stream_impl = {
		.read = random_read,
		.write = random_seed,
};
hlo_stream_t * random_stream_open(void){
	static hlo_stream_t * rng;
	if(!rng){
		rng = hlo_stream_new(&random_stream_impl,NULL,HLO_STREAM_READ);
	}
	return rng;
}
////==========================================================
//Debugging Stream, prints things out
static int debug_write(void * ctx, const void * buf, size_t size){
	DISP("Wrote %d bytes\r\n", size);
	vTaskDelay(2);
	return size;
}
static int debug_read(void * ctx, void * buf, size_t size){
	DISP("Read %d bytes\r\n", size);
	vTaskDelay(2);
	return size;
}
static hlo_stream_vftbl_t debug_stream_impl = {
		.read = debug_read,
		.write = debug_write,
};
hlo_stream_t * debug_stream_open(void){
	static hlo_stream_t * debug;
	if(!debug){
		debug = hlo_stream_new(&debug_stream_impl,NULL,HLO_STREAM_READ_WRITE);
	}
	return debug;
}
////==========================================================
//test commands

