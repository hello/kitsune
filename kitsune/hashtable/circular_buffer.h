#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "boolean.h"

typedef struct {
  unsigned char * begin, *top, *bottom, *end;
} circular_buffer;

void circular_buffer_init( circular_buffer * csb, void * memory, unsigned int size );
boolean circular_buffer_queue( circular_buffer * csb, void * memory, unsigned int size );
boolean circular_buffer_deque( circular_buffer * csb, void * memory, unsigned int * size );

#ifdef __cplusplus
}
#endif

#endif
