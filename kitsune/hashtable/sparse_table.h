#ifndef _SPARSE_TABLE_H
#define _SPARSE_TABLE_H

#include "bit_array.h"
#include "boolean.h"
#include "serialization_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <FreeRTOS.h> /* realloc, free, malloc */
#include <string.h> /* memcpy, memmove */

#define sparse_table_entry void*
typedef struct {
  bit_array presence;
  unsigned char * entries;
  unsigned int size;
  unsigned int entry_sz;
  unsigned int allocated;
} sparse_table;

void sparse_init(sparse_table *st, int size, int entry_sz);
void sparse_dispose(sparse_table *st);
unsigned int sparse_memory_used(sparse_table * st);
unsigned char * sparse_dump(sparse_table *st, unsigned char * memptr);
unsigned char * sparse_lift(sparse_table *st, unsigned char * memptr);
void sparse_remove(sparse_table *st, int idx);
boolean sparse_get(sparse_table *st,  sparse_table_entry value, int idx);
void sparse_update(sparse_table *st, sparse_table_entry entry, int idx );
void sparse_apply(sparse_table *st, boolean(*fp)(sparse_table_entry const*, void * ), void * data );
void sparse_apply_eat(sparse_table *st, void(*fp)(sparse_table_entry const*, void * ), void * data );

#ifdef __cplusplus
}
#endif

#endif
