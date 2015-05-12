#ifndef HLO_ASYNC_H
#define HLO_ASYNC_H
#include "FreeRTOS.h"
#include "semphr.h"
/**
 * tools for asynchronous transaction here
 */
typedef struct{
	int error;
	xSemaphoreHandle sync;
	int buf_size;
	uint8_t buf[0];
}hlo_future_t;

typedef void(*future_task)(hlo_future_t * result, void * ctx);

//standalone creation
hlo_future_t * hlo_future_create(size_t max_size);

//creates and runs the cb on the future task handler.
//future_task is guaranteed to run sequentially
hlo_future_t * hlo_future_create_task(size_t max_size, future_task cb, void * context);

//producer
int hlo_future_write(hlo_future_t * future, const void * buffer, size_t size, int opt_error);


//consumer use this
int hlo_future_read(hlo_future_t * future, void * buf, size_t size);
//or this
int hlo_future_read_with_timeout(hlo_future_t * future,  void * buf, size_t size, TickType_t ms);


//rtos scheduler
void hlo_async_task(void * ctx);

#endif
