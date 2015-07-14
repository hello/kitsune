#include "hlo_queue.h"


typedef struct{
	enum{
		STORE = 0,
		READ
	}type;
	hlo_future_t * sync;
}hlo_queue_task_t;

static void queue_worker_task(void * ctx){
	hlo_queue_t * queue = (hlo_queue_t*)ctx;
	while(1){

	}
	LOGI("Queue worker exited\r\n");
}
hlo_queue_t * hlo_queue_create(const char * root, size_t obj_count){
	hlo_queue_t * ret = pvPortMalloc(sizeof(hlo_queue_t));
	hlo_future_create_task_bg(queue_worker_task,ret,512);
}


int hlo_queue_enqueue(hlo_queue_t * q, void * obj, size_t obj_size, bool sync){

}

hlo_future_t * hlo_queue_dequeue(hlo_queue_t * q){
	hlo_future_t * ret = hlo_future_create();
	assert(ret);
	hlo_queue_task_t = (hlo_queue_task_t){
		.type = READ,
		.sync = ret,
	};
	return ret;
}
