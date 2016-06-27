#include "hlo_async.h"
#include <strings.h>
#include "uart_logger.h"
#include "ustdlib.h"
#include "kit_assert.h"

#define CHECK_FOR_NULL(buf) if(!buf){return -99;}

typedef struct{
	hlo_future_t * result;
	char name[24];
	future_task work;
	void * context;
}async_task_t;

static void hlo_future_free(hlo_future_t * future){
	if( future ) {
		vSemaphoreDelete(future->sync);
		vPortFree(future);
	}
}

static void async_worker(void * ctx){
	if( ctx ) {
		async_task_t * task = (async_task_t*)ctx;
		if(task->work){
			task->work(task->result, task->context);
		}
		//LOGI("\r\n%s stack %d\r\n", task->name, vGetStack( task->name ) );
		if( task->result->release ) {
			vSemaphoreDelete(task->result->release);
		}
		hlo_future_free(task->result);
		vPortFree(task);
	}
	LOGD("async %d stk\n", uxTaskGetStackHighWaterMark(NULL));
	vTaskDelete(NULL);
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
hlo_future_t * hlo_future_create_task_bg(future_task cb, void * context, size_t stack_size){
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
		usnprintf( task->name, sizeof(task->name),  "async %d", xTaskGetTickCount());
		if(pdPASS != xTaskCreate(async_worker, task->name, stack_size / 4, task, 1, NULL)){
			goto fail_task;
		}
	}
	return result;
fail_task:
	vPortFree(task);
fail_sync:
	vSemaphoreDelete(result->release);
fail:
	hlo_future_free(result);
#define FUTURE_FAIL 0
	assert(FUTURE_FAIL);
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
	assert( future );
	if( future ) {
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
#if 0
		//depth first
		hlo_future_read_once(
					hlo_future_create_task_bg(fork,&depth,512),
					&a,
					sizeof(a));
		hlo_future_read_once(
					hlo_future_create_task_bg(fork,&depth,512),
					&b,
					sizeof(b));
#else
		//breadth first
		hlo_future_t * fa, *fb;
		fa = hlo_future_create_task_bg(fork, &depth, 512);
		fb = hlo_future_create_task_bg(fork, &depth, 512);
		hlo_future_read(fa, &a, sizeof(a), portMAX_DELAY);
		hlo_future_read(fb, &b, sizeof(b), portMAX_DELAY);
#endif
		sum = sum + a + b;
		hlo_future_write(result, &sum, sizeof(sum), 0);
#if 0
		//depth first
#else
		//breadth first
		hlo_future_destroy(fa);
		hlo_future_destroy(fb);
#endif
	}


}
int Cmd_FutureTest(int argc, char * argv[]){
	unsigned int depth = 3;
	unsigned int result = 0;
	hlo_future_read_once(
			hlo_future_create_task_bg(fork,&depth,512),
			&result,
			sizeof(result));
	DISP("Res: %d\r\n", result);
	return 0;
}
