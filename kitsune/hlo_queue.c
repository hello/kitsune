#include "hlo_queue.h"
#include "hellofilesystem.h"
#include <string.h>
#include "ustdlib.h"
#include "uart_logger.h"
#include "hlo_async.h"
#include <stdlib.h>
#include "kit_assert.h"
#include "fs_utils.h"

typedef struct{
	enum{
		QUEUE_READ = 0,
		QUEUE_WRITE,
		QUEUE_FLUSH,
		QUEUE_PEEK,
	}type;
	void * buf;
	size_t buf_size;
	hlo_future_t * sync;
	on_finished cleanup;
}worker_context_t;



static char *
_construct_name(char * buf, const char * root, const char * local){
	strcat(buf, "/");
	strcat(buf, root);
	strcat(buf, "/");
	strcat(buf, local);
	return buf;
}
static FRESULT
_open_file(FIL * out_file, const char * root, const char * local_name, WORD mode){
	char name_buf[16+2+13+1] = {0};
	return hello_fs_open(out_file, _construct_name(name_buf, root,  local_name), mode);
}
#define DISK_SECTOR_WRITE_MAX 512
static FRESULT
_write_file(char * root, char * local_name, const char * buffer, WORD size){
	FIL file_obj = {0};
	UINT written = 0;
	UINT bytes = 0;
	FRESULT res = _open_file(&file_obj, root, local_name,  FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
	if(res != FR_OK && res != FR_EXIST){
		LOGE("File %s open fail, code %d", local_name, res);
		return res;
	}
	DISP("writing to %s/%s\r\n", root, local_name);
	do{
		UINT rem = size - written;
		res = hello_fs_write(&file_obj,buffer + written, (rem > DISK_SECTOR_WRITE_MAX ? DISK_SECTOR_WRITE_MAX : rem), &bytes);
		if(res != FR_OK){
			LOGE("unable to write file %d\r\n", res);
			//return res;
		}
		written += bytes;
	}while(written < size);

	res = hello_fs_close(&file_obj);
	if(res != FR_OK){
		LOGE("unable to close file %d\r\n", res);
		return res;
	}
	assert(written == size);
	return FR_OK;
}
static FRESULT
_read_file(char * root, char * local_name, void ** buffer, size_t * size){
	FIL file_obj;
	UINT read = 0;
	FRESULT res = _open_file(&file_obj,root, local_name, FA_READ);
	DISP("Read %s/%s\r\n", root, local_name);
	if(res == FR_OK){

		char * out = pvPortMalloc(file_obj.fsize);
		res = hello_fs_read(&file_obj, out, file_obj.fsize, &read);
		if(FR_OK == res){
			assert(read == file_obj.fsize);
			*buffer = out;
			*size = file_obj.fsize;
		}else{
			DISP("Fail read %d\r\n", res);
			*buffer = NULL;
			*size = 0;
			vPortFree(out);
		}
		res = hello_fs_close(&file_obj);
		if(res != FR_OK){
			LOGE("unable to close read file %d\r\n", res);
		}
	}
	return res;
}
static FRESULT
_delete_file(char * root, char * local_name){
	char name_buf[16+2+13+1] = {0};
	return hello_fs_unlink(_construct_name(name_buf, root, local_name));
}
static void _queue_worker(hlo_future_t * result, void * ctx){
	hlo_queue_t * worker = (hlo_queue_t*)ctx;
	DISP("worker created\r\n");
	int code;
	while(1){
		worker_context_t task;
		memset(&task, 0, sizeof(task));
		if(xQueueReceive(worker->worker_queue, &task, portMAX_DELAY)){
			size_t mcount = uxQueueMessagesWaiting(worker->worker_queue);
			char local_name[13] = {0};
			bool needs_delete = true;

			switch(task.type){
			case QUEUE_PEEK:
				needs_delete = false;
			case QUEUE_READ:
				code = 0;
				ltoa(worker->read_index, local_name);
				if(worker->write_index > worker->read_index){
					if(FR_OK == _read_file(worker->root, local_name, &task.buf, &task.buf_size)){
						if(needs_delete){
							assert(FR_OK == _delete_file(worker->root, local_name));
							worker->read_index++;
							worker->num_files--;
						}
						code = 0;
					}else{
						code = -1;
					}
				}else{
					code = -2;
				}
				hlo_future_write(task.sync, task.buf, task.buf_size, code);
				break;
			case QUEUE_WRITE:
				//writes to flash
				ltoa(worker->write_index, local_name);
				if(task.buf && FR_OK == _write_file(worker->root, local_name, task.buf, task.buf_size)){
					worker->write_index++;
					worker->num_files++;
					code = 0;
				}else{
					code = -1;
				}
				if(task.cleanup){
					task.cleanup(task.buf);
				}
				if(task.sync){
					hlo_future_write(task.sync, NULL, 0, code);
				}
				break;
			default:
			case QUEUE_FLUSH:
				goto fin;
			}
		}
	}
fin:
	hlo_future_write(result, NULL, 0, 0);
	DISP("done\r\n");
}
static void file_itr_delete(void * ctx, const FILINFO * info){
	hlo_queue_t * queue = (hlo_queue_t*)ctx;
	char path[13+3+16] = {0};
	_construct_name(path, queue->root, info->fname);
	assert(FR_OK == hello_fs_unlink(path));
}
static void file_itr_get_min_max(void * ctx, const FILINFO * info){
	hlo_queue_t * queue = (hlo_queue_t*)ctx;
	uint32_t num = strtol(info->fname, NULL, 10);
	//we do not create index of 0
	if(num != 0){
		if(num < queue->read_index){
			queue->read_index = num;
		}
		if(num >= queue->write_index){
			queue->write_index = num + 1;
		}
		queue->num_files++;
	}else if(num == 0){
		//we remove this file because it shouldn't be here
		file_itr_delete(ctx, info);
	}
}

hlo_queue_t * hlo_queue_create(const char * root, size_t max_count, bool clear_all){
	hlo_queue_t * ret = pvPortMalloc(sizeof(*ret));
	memset(ret, 0, sizeof(*ret));
	assert(ret);

	ret->worker_queue = xQueueCreate(10, sizeof(worker_context_t));
	assert(ret->worker_queue);

	ret->worker = hlo_future_create_task_bg(_queue_worker, ret, 2048);
	assert(ret->worker);

	usnprintf(ret->root, sizeof(ret->root),"%s", root);
	hello_fs_mkdir(ret->root);
	LOGI("Created persisted queue: %s\r\n",ret->root);

	ret->read_index = UINT32_MAX;
	ret->write_index = 1;
	ret->num_files = 0;
	ret->max_count = max_count;
	//walk thorough directory for read/write index
	if(clear_all){
		fs_list(root, file_itr_delete, ret);
	}else{
		fs_list(root, file_itr_get_min_max, ret);
	}
	if(ret->num_files == 0){
		ret->read_index = 1;
		ret->write_index = 1;
	}
	DISP("Q r:%u w:%u c:%u\r\n",  ret->read_index, ret->write_index, ret->num_files);
	return ret;
}
//todo make this call thread safe.
void hlo_queue_destroy(hlo_queue_t * q){
	worker_context_t task = (worker_context_t){
		.type = QUEUE_FLUSH,
		.sync = NULL,
		.buf = NULL,
		.buf_size = 0,
		.cleanup = NULL,
	};
	xQueueSend(q->worker_queue,&task,portMAX_DELAY);
	hlo_future_read_once(q->worker, NULL, 0);
	//after this point, the worker is no longer running
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
	if(xQueueSend(q->worker_queue,&task,100)){
		if(task.sync){
			ret = hlo_future_read(task.sync, NULL, 0, portMAX_DELAY);
		}else{
			ret = 0;
		}
	}else{
		LOGE("Queue is full\r\n");
		if(cb){
			cb(obj);
		}
		ret = -1;
	}
	hlo_future_destroy(task.sync);
	return ret;
}

int _queue_read(hlo_queue_t * q, void ** opt_out_obj, size_t * out_size, bool peek_only){
	worker_context_t task = (worker_context_t){
		.type = peek_only?QUEUE_PEEK:QUEUE_READ,
		.sync = hlo_future_create(),
		.buf = NULL,//not used for reads, buf in sync
		.buf_size = 0,
		.cleanup = NULL,
	};
	int ret = 0;
	if(xQueueSend(q->worker_queue, &task, 100)){
		ret = hlo_future_read(task.sync, NULL, 0, portMAX_DELAY);
		if(ret >= 0){
			if(opt_out_obj){
				*opt_out_obj = task.sync->buf;
			}else{
				vPortFree(task.sync->buf);
			}

			if(out_size){
				*out_size = task.sync->buf_size;
			}
		}
	}
	hlo_future_destroy(task.sync);
	return ret;

}
//dequeue is always blocking
int hlo_queue_dequeue(hlo_queue_t * q, void ** opt_out_obj, size_t * out_size){
	return _queue_read(q, opt_out_obj, out_size, 0);
}
int hlo_queue_peek(hlo_queue_t * q, void ** opt_out_obj, size_t * out_size){
	return _queue_read(q, opt_out_obj, out_size, 1);
}
#include "fs_utils.h"
void _on_enqueue_free(void * buf){
	vPortFree(buf);
}
static int _test_queue(char * root){
	hlo_queue_t * q = hlo_queue_create(root, 3, 1);
	char * out_string = NULL;
	int fcount = 0;
	size_t out_size;

	char * nb_str = strdup("nonblocking");
	assert(0 == hlo_queue_enqueue(q, nb_str, strlen(nb_str)+1, 0, _on_enqueue_free));

	char * blocking_str = strdup("blocking");
	assert(0 == hlo_queue_enqueue(q, blocking_str, strlen(blocking_str)+1, 1, NULL));
	vPortFree(blocking_str);

	vTaskDelay(200);
	assert(0 == hlo_queue_dequeue(q, (void**)&out_string, &out_size));
	DISP("Dequeue %s\r\n", out_string);
	memset(out_string, 0, out_size);
	vPortFree(out_string);

	assert(0 == hlo_queue_peek(q,  (void**)&out_string, &out_size));
	DISP("Peek %s\r\n", out_string);
	memset(out_string, 0, out_size);
	vPortFree(out_string);

	assert(0 == hlo_queue_dequeue(q, (void**)&out_string, &out_size));
	DISP("Dequeue %s\r\n", out_string);
	memset(out_string, 0, out_size);
	vPortFree(out_string);

	assert(-2 == hlo_queue_dequeue(q, NULL, NULL));
	DISP("Dequeue NULL \r\n", out_string);

	fs_list(root, file_itr_counter, &fcount);
	//assert(fcount == 0);

	hlo_queue_destroy(q);
	return 0;
}
int Cmd_Hlo_Queue_Test(int argc, char * argv[]){
	int i;
	for(i = 0; i< 50; i++){
		assert(0 == _test_queue("test"));
		vTaskDelay(500);
	}
	return 0;
}
