#ifndef HLO_ASYNC_H
#define HLO_ASYNC_H
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
/**
 * tools for asynchronous transaction here
 */
typedef struct hlo_future_t{
	hlo_future_t * write_lock;
	int return_code;
	xSemaphoreHandle sync;
	int buf_size;
	void * buf;
}hlo_future_t;

typedef int(*future_task)(void * out_buf, size_t out_size, void * ctx);

//standalone creation
hlo_future_t * hlo_future_create(size_t max_size);

/**
 * Adds the callback to @see hlo_async_task queue, calls to this api are guaranteed to be run sequentially
 * @param max_size - size of the future objects internal buffer for write/read
 * @param cb - the task to be scheduled
 * @param context - context the callback
 *
 * @return a new object if everything checks out.
 */
hlo_future_t * hlo_future_create_task(size_t max_size, future_task cb, void * context);

/**
 * Creates a thread to run the callback.
 * @param max_size - size of the future object's internal buffer for write/read
 * @param cb - the the callback will be run in a disposable thread
 * @param context - pointer to any context to be run alongside cb
 * @param stack_size - the stack size of the disposable thread
 *
 * @return a new object if everything checks out.
 */
hlo_future_t * hlo_future_create_task_bg(size_t max_size, future_task cb, void * context, size_t stack_size);

/**
 *  @param future - the future you wish to read from
 *  @param buf -  copies the content of the future's internal buffer into this
 *  @param size - size of your buffer
 *  @param ms - delay to wait
 *  @return the return code set by hlo_future_write.  Check with implementer, but typically < 0 are errros
 */
int hlo_future_read(hlo_future_t * future,  void * buf, size_t size, TickType_t ms);
/**
 * helper api
 * hlo_future_read with automatic self destruction.
 * always delay at maximum time.
 */
int hlo_future_read_once(hlo_future_t * future,  void * buf, size_t size);
/**
 * destroys a future
 * use after a read, otherwise it'll crash the system.
 */
void hlo_future_destroy(hlo_future_t * future);

/**
 * run this to support synchronous mode aka @see hlo_future_create_task
 */
void hlo_async_task(void * ctx);


//helper macros

#endif
