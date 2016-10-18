#include "hlo_stream.h"

////==========================================================
//circular buffer stream, no optimization, no thread safety
typedef struct{
	volatile int write_idx;
	volatile int read_idx;
	volatile int filled;
	int capacity;
	uint8_t buf[0];
} hlo_circbuf_stream_t;


hlo_stream_t * hlo_circbuf_stream_open(size_t capacity);
