#include "hlo_async.h"
#include <strings.h>


#define CHECK_FOR_NULL(buf) if(!buf){return -99;}
static void hlo_future_destroy(hlo_future_t * future){
	if(future){
		vSemaphoreDelete(future->sync);
		vPortFree(future);
	}
}
hlo_future_t * hlo_future_create(size_t max_size){
	size_t alloc_size = sizeof(hlo_future_t) + max_size;
	hlo_future_t * ret = pvPortMalloc(alloc_size);
	if(ret){
		memset(ret, 0, alloc_size);
		ret->sync = xSemaphoreCreateBinary();
		ret->buf_size = (int)max_size;
	}
	return ret;
}

//producer use either
int hlo_future_write(hlo_future_t * future, const void * buffer, size_t size, int opt_error){
	CHECK_FOR_NULL(future);
	int err = 0;
	if(future->buf_size >= size){
		memcpy(future->buf,buffer,size);
		future->buf_size = size;
	}else{
		//input too bigs
		err = -1;
	}
	future->error = opt_error;
	xSemaphoreGive(future->sync);
	return err;
}
static int do_read(hlo_future_t * future, void * buf, size_t size){
	int err = 0;
	if(future->error < 0){
		//has error, do not copy
		err = future->error;
	}else if(size < future->buf_size){
		//not enough space
		err = -1;
	}else{
		memcpy(buf,future->buf,future->buf_size);
		err = future->buf_size;
	}
	return err;
}
int hlo_future_read(hlo_future_t * future, void * buf, size_t size){
	CHECK_FOR_NULL(future);
	xSemaphoreTake(future->sync,portMAX_DELAY);
	int err = do_read(future, buf, size);
	hlo_future_destroy(future);
	return err;
}
//or this
int hlo_future_read_with_timeout(hlo_future_t * future,  void * buf, size_t size, int ms){
	CHECK_FOR_NULL(future);
	TickType_t poll_delay = ms>10?10:ms;
	int err = -11;
	while(ms > 0){
		if(xSemaphoreTake(future->buf,poll_delay) == pdTRUE){
			err = do_read(future, buf, size);
			break;
		}else{
			ms -= poll_delay;
		}
	}
	if(ms < 0){
		//timed out, do not free here
		//todo: defer this object to be freed later
	}else{
		hlo_future_destroy(future);
	}
	return err;
}
