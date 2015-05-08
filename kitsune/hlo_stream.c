#include "hlo_stream.h"
#include "kit_assert.h"
#include <strings.h>
#include "uart_logger.h"

#define LOCK(stream) xSemaphoreTake(stream->info.lock, portMAX_DELAY)
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
	if(ret == 0){
		vSemaphoreDelete(stream->info.lock);
		vPortFree(stream);
	}
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
	vSemaphoreCreateBinary(stream->info.lock);
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
		return HLO_STREAM_EAGAIN;
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
		return HLO_STREAM_EAGAIN;
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
//Random Stream
void get_random(int num_rand_bytes, uint8_t *rand_data);
static int random_read(void * ctx, void * buf, size_t size){
	get_random(size,(uint8_t*)buf);
	return size;
}
static hlo_stream_vftbl_t random_stream_impl = {
		.read = random_read,
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

#include "hellofilesystem.h"
static hlo_stream_t *user_streams[3];

int Cmd_make_stream(int argc, char *argv[]){
	DISP("Test: Making fifo 0");
	//user_streams[0] = fifo_stream_open(16);
	if(argc > 1){
		user_streams[0] = fs_stream_open(argv[1], HLO_STREAM_READ);
	}
	return 0;
}
int Cmd_write_stream(int argc, char *argv[]){
	DISP("Test: Writing fifo 0");
	if(argc > 1){
		hlo_stream_t * stream = user_streams[0];

		int ret = hlo_stream_write(stream, argv[1], strlen(argv[1]));
		DISP("write %s, return code %d\r\n",argv[1], ret);
		hlo_stream_info(stream);
	}
	return 0;
}
int Cmd_read_stream(int argc, char *argv[]){
	if(argc > 0){
		uint8_t temp[9] = {0};
		hlo_stream_t * stream = user_streams[0];

		int ret = hlo_stream_read(stream, temp, 8);
		DISP("read %s, return code %d\r\n", temp, ret);
		hlo_stream_info(stream);
	}
	return 0;
}
