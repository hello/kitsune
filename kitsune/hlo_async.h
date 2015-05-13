#ifndef HLO_ASYNC_H
#define HLO_ASYNC_H
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
/**
 * tools for asynchronous transaction here
 */
typedef struct{
	int return_code;
	xSemaphoreHandle sync;
	int buf_size;
	uint8_t buf[0];
}hlo_future_t;

typedef void(*future_task)(hlo_future_t * result, void * ctx);

//standalone creation
hlo_future_t * hlo_future_create(size_t max_size);

//creates and runs the cb on the future task handler with a large stack
//future_task is guaranteed to run sequentially
//used to conserve space
hlo_future_t * hlo_future_create_task(size_t max_size, future_task cb, void * context);

//creates a new thread to run the task
//not guarantee for sequentiality
hlo_future_t * hlo_future_create_task_bg(size_t max_size, future_task cb, void * context, size_t stack_size);


//producer
int hlo_future_write(hlo_future_t * future, const void * buffer, size_t size, int return_code);

//consumer use this
int hlo_future_read(hlo_future_t * future,  void * buf, size_t size, TickType_t ms);

//destroy after read
void hlo_future_destroy(hlo_future_t * future);

//rtos scheduler
void hlo_async_task(void * ctx);


//helper macros

#endif
