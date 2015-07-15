#include "hlo_queue.h"


typedef struct{
	enum{
		QUEUE_READ = 0,
		QUEUE_WRITE,
		QUEUE_FLUSH,
	}operation;
	void * buf;
	size_t buf_size;
	on_finished cleanup;
}worker_context_t;

static char*
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
}
static void _queue_worker(void * ctx){
	hlo_queue_t * worker = (hlo_queue_t*)ctx;
	xQueueHandle obj_cache = xQueueCreate(worker->watermark, sizeof(worker_context_t));
	assert(obj_cache);
	while(1){
		worker_context_t task = {0};
		if(xQueueReceive(worker->worker_queue, &task, portMAX_DELAY)){
			switch(task.operation){
			case QUEUE_READ:
				if(worker->write_index > worker->read_index){
					//read from flash
					//allocate
				}else{
					//read from cache
					//do not need to allocate
				}
				//signal finished
				break;
			case QUEUE_WRITE:
				while(xQueueSend(obj_cache, &task,1) != pdTRUE){
					//cache is full
					worker_context_t cached_task = {0};
					if(xQueueReceive(obj_cache, &cached_task, portMAX_DELAY) == pdTRUE){
						//write to flash
						assert(FR_OK == _write_file(worker->root, "0", cached_task.buf, cached_task.buf_size));
						cached_task.cleanup(cached_task.buf);
					}
				}
				//signal done

				break;
			case QUEUE_FLUSH:
				goto fin;
			default:
				assert(0);
				break;
			}
		}
	}
fin:
	//flush cache
	while(uxQueueMessagesWaiting(obj_cache) > 0){
		worker_context cached_task = {0};
		if(xQueueReceive(obj_cache, &cached_task, portMAX_DELAY) == pdTRUE){
			//write to flash
			_write_file(worker->root, "0", cached_task.buf, cached_task.buf_size)
		}
	}
	hlo_future_write(worker->worker, NULL, 0, 0);
	vQueueDelete(obj_cache);
}
hlo_queue_t * hlo_queue_create(const char * root, size_t obj_count, size_t watermark){
	hlo_queue_t * ret = pvPortMalloc(sizeof(*ret));
	memset(ret, 0, sizeof(*ret));
	assert(ret);
	usnprintf(ret->root, sizeof(ret->root),"%s", root);
	ret->worker = hlo_future_create_task_bg(_queue_worker, ret, 512);
	ret->worker_queue = xQueueCreate(10, sizeof(worker_context_t));
	//walk thorough directory for read/write index
	return ret;
}


int hlo_queue_enqueue(hlo_queue_t * q, void * obj, size_t obj_size, on_finished cb){
	if(xQueue)

}

int hlo_queue_dequeue(hlo_queue_t * q, void ** out_obj, size_t * out_size){

}
