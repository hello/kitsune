#ifndef HLO_QUEUE_H
#define HLO_QUEUE_H
#include "hlo_async.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdbool.h>
/*
 * persistend fifo queue
 */
typedef struct{
	char root[16];				//directory to store serialized objects
	size_t max_obj_count;		//max number of objects in the directory
	hlo_future_t * worker;		//used for the bg thread
	xQueueHandle worker_queue;	//queue for r/w access
	uint32_t write_index;
	uint32_t read_index;
	uint16_t max_count;			//# of objects max to keep in root
	uint32_t num_files;			//number of files in the directory
}hlo_queue_t;

typedef void (*on_finished)(void * buf);

hlo_queue_t * hlo_queue_create(const char * root, size_t max_count, bool clear_all);
void hlo_queue_destroy(hlo_queue_t * q);
/**
 *
 */
int hlo_queue_enqueue(hlo_queue_t * q, void * obj, size_t obj_size, bool blocking, on_finished cb);

int hlo_queue_dequeue(hlo_queue_t * q, void ** out_obj, size_t * out_size);


int hlo_queue_count(hlo_queue_t * q);

int Cmd_Hlo_Queue_Test(int argc, char * argv[]);

#endif
