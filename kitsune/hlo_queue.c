#include "hlo_queue.h"
#include "hellofilesystem.h"
#include <string.h>
#include "ustdlib.h"
#include "uart_logger.h"
#include "hlo_async.h"
#include "kit_assert.h"

typedef struct{
	enum{
		QUEUE_READ = 0,
		QUEUE_WRITE,
		QUEUE_FLUSH,
	}type;
	void * buf;
	size_t buf_size;
	hlo_future_t * sync;
	on_finished cleanup;
}worker_context_t;

/*static char*
_construct_name(char * buf, const char * root, const char * local){
	strcat(buf, "/");
	strcat(buf, root);
	strcat(buf, "/");
	strcat(buf, local);
	return buf;
}
static FRESULT
_open_file(FIL * out_file, const char * root, const char * local_name, WORD mode){
	char name_buf[32] = {0};
	return hello_fs_open(out_file, _construct_name(name_buf, root,  local_name), mode);
}
static FRESULT
_write_file(char * root, char * local_name, const char * buffer, WORD size){
	FIL file_obj;
	UINT bytes = 0;
	WORD written = 0;
	FRESULT res = _open_file(&file_obj, root, local_name, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
	if(res != FR_OK && res != FR_EXIST){
		LOGE("File %s open fail, code %d", local_name, res);
		return res;
	}
	do{
		res = hello_fs_write(&file_obj, buffer + written, SENSE_LOG_RW_SIZE, &bytes);
		written += bytes;
	}while(written < size);
	res = hello_fs_close(&file_obj);
	if(res != FR_OK){
		LOGE("unable to write queue file\r\n");
		return res;
	}
	return FR_OK;
}*/
static void _queue_worker(hlo_future_t * result, void * ctx){
	hlo_queue_t * worker = (hlo_queue_t*)ctx;
	while(1){
		worker_context_t task = (worker_context_t){0};
		while(xQueueReceive(worker->worker_queue, &task, portMAX_DELAY)){
			size_t mcount = uxQueueMessagesWaiting(worker->worker_queue);
			switch(task.type){
			case QUEUE_READ:
				if(1){
					//read from queue
				}else{
					//no op
				}
				hlo_future_write(task.sync, NULL, 0, 0);
				break;
			case QUEUE_WRITE:
				//writes to flash

				//free
				if(task.cleanup){
					task.cleanup(task.sync);
				}
				if(task.sync){
					hlo_future_write(task.sync, NULL, 0, 0);
				}

				break;
			case QUEUE_FLUSH:
				goto fin;
			}
		}
	}
fin:
	//flush stuff here
	hlo_future_write(result, NULL, 0, 0);
	LOGI("Flushing worker queue\r\n");
}
hlo_queue_t * hlo_queue_create(const char * root, size_t obj_count, size_t watermark){
	hlo_queue_t * ret = pvPortMalloc(sizeof(*ret));
	memset(ret, 0, sizeof(*ret));
	assert(ret);
	usnprintf(ret->root, sizeof(ret->root),"%s", root);
	ret->worker = hlo_future_create_task_bg(_queue_worker, ret, 1024);
	ret->worker_queue = xQueueCreate(10, sizeof(worker_context_t));
	//walk thorough directory for read/write index
	return ret;
}
//todo make this call thread safe.
void hlo_queue_destroy(hlo_queue_t * q){
	worker_context_t task = (worker_context_t){
		.type = QUEUE_FLUSH,
	};
	xQueueSend(q,&task,portMAX_DELAY);
	hlo_future_read_once(q->worker, NULL, 0);
	vQueueDelete(q->worker_queue);
	vPortFree(q);
	LOGI("done\r\n");

}

int hlo_queue_enqueue(hlo_queue_t * q, void * obj, size_t obj_size, bool blocking, on_finished cb){
	worker_context_t task = (worker_context_t){
		.type = QUEUE_WRITE,
		.sync = blocking ? hlo_future_create():NULL,
		.buf = obj,
		.buf_size = obj_size,
		.cleanup = cb,
	};
	int ret = 0;
	if(xQueueSend(q,&task,100)){
		hlo_future_read(task.sync, NULL, 0, portMAX_DELAY);
	}else{
		LOGE("Queue is full\r\n");
		ret = -1;
	}
	hlo_future_destroy(task.sync);
	return ret;
}

//dequeue is always blocking
int hlo_queue_dequeue(hlo_queue_t * q, void ** out_obj, size_t * out_size){
	worker_context_t task = (worker_context_t){
		.type = QUEUE_READ,
		.sync = hlo_future_create(),
		.buf = NULL,
		.buf_size = 0,
		.cleanup = NULL
	};
	int ret;
	if(xQueueSend(q, &task, 100)){
		ret = hlo_future_read(task.sync, *out_obj, *out_size, portMAX_DELAY);
	}else{
		ret = -1;
	}
	hlo_future_destroy(task.sync);
	return ret;

}

int Cmd_Hlo_Queue_Test(int argc, char * argv[]){
	hlo_queue_t * q = hlo_queue_create("test", 10, 3);
	hlo_queue_destroy(q);
	return 0;
}
