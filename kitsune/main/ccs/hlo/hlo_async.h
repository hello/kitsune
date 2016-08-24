#ifndef HLO_ASYNC_H
#define HLO_ASYNC_H
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
/**
 * tools for asynchronous transaction here
 */
typedef struct hlo_future_t{
	xSemaphoreHandle release;
	xSemaphoreHandle sync;
	int return_code;
	int buf_size;
	void * buf;
}hlo_future_t;

/**
 * LifeCycle of the task
 * 1. allocate local resources
 * 2. compute
 * 3. capture value that's accessible in the current scope
 * 4. deallocate local resources
 */
typedef void(*future_task)(hlo_future_t * result, void * ctx);

//standalone creation
hlo_future_t * hlo_future_create(void);

/**
 * Creates a thread to run the callback.
 * @param max_size - size of the future object's internal buffer for write/read
 * @param cb - the the callback will be run in a disposable thread
 * @param context - pointer to any context to be run alongside cb
 * @param stack_size - the stack size of the disposable thread
 *
 * @return a new object if everything checks out.
 */
hlo_future_t * hlo_future_create_task_bg(future_task cb, void * context, size_t stack_size);
/**
 * Binds a region of memory, its size, and return code to the future so that readers can access it.
 * If called inside a future_task, it halts the task until a reader invokes the destroy method.
 * @param future - future object
 * @param buffer - pointer to the object
 * @param size -size of the object
 * @param return_code - return code
 */
void hlo_future_write(hlo_future_t * future, void * buffer, size_t size, int return_code);
/**
 *  copies the content of the future into the supplied buffer.
 *  @param future - the future you wish to read from
 *  @param buf -  NON-NULL to copy the future content into here, NULL to wait until finished.
 *  @param size - size of your buffer
 *  @param ms - delay to wait
 *  @return the return code set by hlo_future_write.  Check with implementer, but typically < 0 are errros
 */
int hlo_future_read(hlo_future_t * future,  void * buf, size_t size, TickType_t ms);
/**
 * helper api, reads then deallocate the future
 * hlo_future_read with automatic self destruction.
 * always delay at maximum time.
 */
int hlo_future_read_once(hlo_future_t * future,  void * buf, size_t size);
/**
 * destroys a future, contents inside are considered invalid after this call.
 */
void hlo_future_destroy(hlo_future_t * future);
int Cmd_FutureTest(int argc, char * argv[]);
#endif
