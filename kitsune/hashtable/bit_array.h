#ifndef _BIT_ARRAY_H
#define _BIT_ARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned char * bits;
  int size;
} bit_array;

#include "stdbool.h"

void bit_array_resize(bit_array* ba, int size);
void bit_array_init(bit_array* ba, int size);
void bit_array_dispose(bit_array* ba);
void bit_array_put(bit_array* ba, int idx, bool val);
int bit_array_clz(bit_array* ba );
void bit_array_rshift_bytes(bit_array* ba, int s);
bool bit_array_get(bit_array* ba, int idx);
unsigned int bit_array_count(bit_array* ba, int idx);
unsigned int bit_array_memory_used(bit_array * ba);
unsigned char* bit_array_dump( bit_array* ba, unsigned char * memptr );
unsigned char* bit_array_lift( bit_array* ba, unsigned char * memptr );

#ifdef __cplusplus
}
#endif 

#endif
