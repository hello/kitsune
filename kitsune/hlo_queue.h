#ifndef HLO_QUEUE_H
#define HLO_QUEUE_H
#include "hlo_async.h"
#include "FreeRTOS.h"
#include "queue.h"
/*
 * persistend fifo queue
 */
typedef struct{
	char root[16];
	size_t max_obj_count;
	xQueueHandle task_queue;
	hlo_future_t * task_worker;
}hlo_queue_t;

hlo_queue_t * hlo_queue_create(const char * root, size_t obj_count);

/**
 * if sync is set to true, this blocks until the object is commited to flash
 */
int hlo_queue_enqueue(hlo_queue_t * q, void * obj, size_t obj_size, bool sync);

hlo_future_t * hlo_queue_dequeue(hlo_queue_t * q);

#endif
