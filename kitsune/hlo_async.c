#include "hlo_async.h"
#include <strings.h>
#include "uart_logger.h"
#include "ustdlib.h"

#define CHECK_FOR_NULL(buf) if(!buf){return -99;}

typedef struct{
	hlo_future_t * result;
	char name[24];
	future_task work;
	void * context;
}async_task_t;

static void hlo_future_write(hlo_future_t * future, int return_code){
	future->return_code = return_code;
	xSemaphoreGive(future->sync);
}
static void async_worker(void * ctx){
	async_task_t * task = (async_task_t*)ctx;
	if(task->work){
		hlo_future_write(task->result,
				task->work(task->result->buf,task->result->buf_size, task->context));
	}
	LOGI("\r\n%s stack %d\r\n", task->name, vGetStack( task->name ) );

	vPortFree(task);
	vTaskDelete(NULL);
}
static int do_read(hlo_future_t * future, void * buf, size_t size){
	if(buf && size >= future->buf_size){
		memcpy(buf,future->buf,future->buf_size);
	}
	return future->return_code;
}


////-------------
//public
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
hlo_future_t * hlo_future_create_task_bg(size_t max_size, future_task cb, void * context, size_t stack_size){
	hlo_future_t * result = hlo_future_create(max_size);
	async_task_t * task;
	if(result){
		task = pvPortMalloc(sizeof(async_task_t));
		if(!task){
			goto fail;
		}
		task->context = context;
		task->result = result;
		task->work = cb;

		usnprintf( task->name, sizeof(task->name),  "async %d", xTaskGetTickCount());
		if(pdPASS != xTaskCreate(async_worker, task->name, stack_size / 4, task, 4, NULL)){
			goto fail_task;
		}
	}
	return result;
fail_task:
	vPortFree(task);
fail:
	hlo_future_destroy(result);
	return NULL;
}

void hlo_future_destroy(hlo_future_t * future){
	if(future){
		vSemaphoreDelete(future->sync);
		vPortFree(future);
	}
}
//or this
int hlo_future_read(hlo_future_t * future,  void * buf, size_t size, TickType_t ms){
	CHECK_FOR_NULL(future);
	int err = -11;
	if(xSemaphoreTake(future->sync, ms) == pdTRUE){
		err = do_read(future, buf, size);
		xSemaphoreGive(future->sync);
	}
	return err;
}
int hlo_future_read_once(hlo_future_t * future,  void * buf, size_t size){
	int res = hlo_future_read(future, buf, size, portMAX_DELAY);
	hlo_future_destroy(future);
	return res;
}
