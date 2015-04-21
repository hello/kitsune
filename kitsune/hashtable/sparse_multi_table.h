#ifndef _SPARSE_MULTI_TABLE_H
#define _SPARSE_MULTI_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sparse_table.h"
#include "serialization_macros.h"

#include <stdlib.h> /* free, malloc */

// Here begins the wrapper around multiple sparse tables
typedef struct {
  sparse_table * entries;
  unsigned int * offsets;
  unsigned int size;
  unsigned int num_tbls;
} sparse_multi_table;

boolean sparse_multi_get(sparse_multi_table* smt,  sparse_table_entry value, int idx);
void sparse_multi_update(sparse_multi_table *smt, sparse_table_entry entry, int idx);
void sparse_multi_remove(sparse_multi_table *smt, int idx);
void sparse_multi_apply(sparse_multi_table *smt, boolean(*fp)(sparse_table_entry const*, void * ), void * data );
void sparse_multi_apply_eat(sparse_multi_table *smt, void(*fp)(sparse_table_entry const*, void * ), void * data );
void sparse_multi_init(sparse_multi_table* smt, int size, int entry_sz);
void sparse_multi_dispose(sparse_multi_table *smt);
unsigned int sparse_multi_memory_used(sparse_multi_table * smt);
unsigned char * sparse_multi_dump(sparse_multi_table *smt, unsigned char * memptr);
unsigned char * sparse_multi_lift(sparse_multi_table *smt, unsigned char * memptr);

#ifdef __cplusplus
}
#endif

#endif

