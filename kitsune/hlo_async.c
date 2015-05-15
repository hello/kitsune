#include "hlo_async.h"
#include <strings.h>
#include "uart_logger.h"
#include "ustdlib.h"

#define CHECK_FOR_NULL(buf) if(!buf){return -99;}

typedef struct{
	hlo_future_t * result;
	future_task work;
	void * context;
}async_task_t;

static void hlo_future_free(hlo_future_t * future){
	vSemaphoreDelete(future->sync);
	vPortFree(future);
}
#define WORKER_TASK_NAME "future_worker"
static void async_worker(void * ctx){
	async_task_t * task = (async_task_t*)ctx;
	if(task->work){
		task->work(task->result, task->context);
	}
	//LOGI("\r\n%s stack %d\r\n", WORKER_TASK_NAME, vGetStack( WORKER_TASK_NAME ) );
	vSemaphoreDelete(task->result->release);
	hlo_future_free(task->result);
	vPortFree(task);
}
static int do_read(hlo_future_t * future, void * buf, size_t size){
	if(buf && future->buf && size >= future->buf_size){
		memcpy(buf,future->buf,future->buf_size);
	}
	return future->return_code;
}


////-------------
//public
hlo_future_t * hlo_future_create(void){
	hlo_future_t * ret = pvPortMalloc(sizeof(hlo_future_t));
	if(ret){
		memset(ret, 0, sizeof(hlo_future_t));
		ret->sync = xSemaphoreCreateBinary();
		//future defaults to fail if not captured
		ret->return_code = -88;
	}
	return ret;
}




static xQueueHandle future_proc_q = NULL;
static xTaskHandle future_proc_task = NULL;
static void task_process_future(void* params) {
	async_task_t * task;
	xQueueHandle q = (xQueueHandle)params;
	while( xQueueReceive(q, &task, 0 ) ) {
		async_worker(task);
		vPortFree(task);
	}
	future_proc_task = NULL;
	vTaskDelete(NULL);
}


hlo_future_t * hlo_future_create_task_bg(future_task cb, void * context){
	#define MAX_FUTURES_QUEUED 32
	hlo_future_t * result = hlo_future_create();
	async_task_t * task;
	if(result){
		task = pvPortMalloc(sizeof(async_task_t));
		if(!task){
			goto fail;
		}
		//this lock indicates it's running a background thread and captures values
		result->release = xSemaphoreCreateBinary();
		if(!result->release){
			goto fail_sync;
		}

		task->context = context;
		task->result = result;
		task->work = cb;

		if( !future_proc_q ) {
			future_proc_q = xQueueCreate(MAX_FUTURES_QUEUED, sizeof(async_task_t*));
			if(!future_proc_q) {
				goto fail_task;
			}
		}

		xQueueSend(future_proc_q, (void* )&task, portMAX_DELAY);
		if( future_proc_task == NULL ) {
			if(pdPASS != xTaskCreate(task_process_future, WORKER_TASK_NAME, 4*1024 / 4, future_proc_q, 4, &future_proc_task) ) {
				goto fail_task;
			}
		}
	}
	return result;
fail_task:
	vPortFree(task);
fail_sync:
	vSemaphoreDelete(result->release);
fail:
	hlo_future_free(result);
	return NULL;
}

void hlo_future_destroy(hlo_future_t * future){
	if(future){
		if(future->release){
			//running in a background thread
			xSemaphoreGive(future->release);
		}else{
			hlo_future_free(future);
		}
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
void hlo_future_write(hlo_future_t * future, void * buffer, size_t size, int return_code){
	future->return_code = return_code;
	future->buf = buffer;
	future->buf_size = size;
	//allow reader to access data
	xSemaphoreGive(future->sync);
	//halts the current thread until released
	if(future->release){
		xSemaphoreTake(future->release, portMAX_DELAY);
	}
}

static void fork(hlo_future_t * result, void * ctx){
	unsigned int depth = *(unsigned int*)ctx;
	unsigned int sum = depth;
	DISP("(%d)", depth);
	if(depth == 1){
		hlo_future_write(result, &sum, sizeof(sum),0);
	}else{
		depth--;
		unsigned int a,b;
		hlo_future_read_once(
					hlo_future_create_task_bg(fork,&depth),
					&a,
					sizeof(a));
		hlo_future_read_once(
					hlo_future_create_task_bg(fork,&depth),
					&b,
					sizeof(b));
		sum = sum + a + b;
		hlo_future_write(result, &sum, sizeof(sum), 0);
	}


}
int Cmd_FutureTest(int argc, char * argv[]){
	unsigned int depth = 3;
	unsigned int result = 0;
	hlo_future_read_once(
			hlo_future_create_task_bg(fork,&depth),
			&result,
			sizeof(result));
	DISP("Res: %d\r\n", result);
	return 0;
}
