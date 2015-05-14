#include "hlo_async.h"
#include <strings.h>


#define CHECK_FOR_NULL(buf) if(!buf){return -99;}
static xQueueHandle _queue;

typedef struct{
	hlo_future_t * result;
	future_task work;
	void * context;
}async_task_t;

void hlo_async_task(void * ctx){
	_queue = xQueueCreate(16,sizeof( async_task_t ) );
	async_task_t task;
	while(1){
		  xQueueReceive(_queue,(void *) &task, portMAX_DELAY );
		  task.work(task.result, task.context);
	}
}
static void async_worker(void * ctx){
	async_task_t * task = (async_task_t*)ctx;
	if(task->work){
		task->work(task->result, task->context);
	}
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
hlo_future_t * hlo_future_create_task(size_t max_size, future_task cb, void * context){
	if(!_queue){
		return NULL;
	}
	hlo_future_t * result = hlo_future_create(max_size);
	if(result){
		async_task_t task = (async_task_t){
			.result = result,
			.work = cb,
			.context = context,
		};
		xQueueSend(_queue,&task,0);
	}
	return result;

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
		if(pdPASS != xTaskCreate(async_worker, "asyncWorker", stack_size / 4, task, 4, NULL)){
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
//producer use either
int hlo_future_write(hlo_future_t * future, const void * buffer, size_t size, int return_code){
	CHECK_FOR_NULL(future);
	int err = 0;
	if(future->buf_size >= size){
		if(buffer){
			memcpy(future->buf,buffer,size);
		}
	}else{
		//input too bigs
		err = -1;
	}
	future->return_code = return_code;
	xSemaphoreGive(future->sync);
	return err;
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
