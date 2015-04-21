#include "sparse_multi_table.h"
#include "stdint.h"

 /* 64 bit fixed point division gives an *exact* result because we're not interested in fractional part after the multiply. */
#define MULTI_WRAP_MATH \
  int n = (((((int64_t)idx<<32)/(int64_t)smt->size)) * (int64_t)smt->num_tbls)>>32;\
  idx -= smt->offsets[n];

#define MULTI_WRAP_RETURN(name, ...) MULTI_WRAP_MATH return name(__VA_ARGS__);
#define MULTI_WRAP_VOID(name, ...) MULTI_WRAP_MATH name(__VA_ARGS__);

#define NUM_TBLS(size) size >> 10 == 0 ? 1 : size >> 10;//change this line to tune space - speed tradeoff

boolean sparse_multi_get(sparse_multi_table* smt,  sparse_table_entry value, int idx) {
  MULTI_WRAP_RETURN(sparse_get, &smt->entries[n], value, idx);
}
void sparse_multi_update(sparse_multi_table *smt, sparse_table_entry entry, int idx) {
  MULTI_WRAP_VOID(sparse_update, &smt->entries[n], entry, idx);
}
void sparse_multi_remove(sparse_multi_table *smt, int idx) {
  MULTI_WRAP_VOID(sparse_remove, &smt->entries[n], idx);
}
void sparse_multi_apply(sparse_multi_table *smt, boolean(*fp)(sparse_table_entry const*, void * ), void * data ) {
  unsigned int i;
  for(i=0;i<smt->num_tbls;++i)
    sparse_apply(&smt->entries[i], fp, data);
}
void sparse_multi_apply_eat(sparse_multi_table *smt, void(*fp)(sparse_table_entry const*, void * ), void * data ) {
  unsigned int i;
  for(i=0;i<smt->num_tbls;++i) {
    sparse_apply_eat(&smt->entries[i], fp, data);
    }
}
void sparse_multi_init(sparse_multi_table* smt, int size, int entry_sz) {
  unsigned int i;
  smt->size = size;
  smt->num_tbls = NUM_TBLS(smt->size);
  smt->entries = (sparse_table *)pvPortMalloc(smt->num_tbls*sizeof(sparse_table));
  smt->offsets = (unsigned int *)pvPortMalloc(smt->num_tbls*sizeof(unsigned int));
  for(i=0;i<smt->num_tbls;++i) {
    sparse_init(&smt->entries[i], size/smt->num_tbls, entry_sz);
    smt->offsets[i] = smt->size / smt->num_tbls * i;
  }
}
void sparse_multi_dispose(sparse_multi_table *smt) {
  unsigned int i;
  for(i=0;i<smt->num_tbls;++i)
    sparse_dispose(&smt->entries[i]);
  FREE_AND_NULL(smt->entries);
  FREE_AND_NULL(smt->offsets);
}
unsigned int sparse_multi_memory_used(sparse_multi_table * smt) {
  unsigned int i;
  unsigned int size = sizeof(smt->num_tbls) + sizeof(smt->size);
  for(i=0;i<smt->num_tbls;++i)
    size += sparse_memory_used(&smt->entries[i]);
  return size;
}
unsigned char * sparse_multi_dump(sparse_multi_table *smt, unsigned char * memptr) {
  unsigned int i;
  DUMP_ADVANCE(smt->num_tbls, memptr);
  DUMP_ADVANCE(smt->size, memptr);
  for(i=0;i<smt->num_tbls;++i)
    memptr = sparse_dump(&smt->entries[i], memptr);
  return memptr;
}
unsigned char * sparse_multi_lift(sparse_multi_table *smt, unsigned char * memptr) {
  unsigned int i;
  memset(smt, 0, sizeof(sparse_multi_table));
  LIFT_ADVANCE(smt->num_tbls, memptr);
  LIFT_ADVANCE(smt->size, memptr);
  smt->entries = (sparse_table *)pvPortMalloc(smt->num_tbls*sizeof(sparse_table));
  smt->offsets = (unsigned int *)pvPortMalloc(smt->num_tbls*sizeof(unsigned int));
  for(i=0;i<smt->num_tbls;++i) {
    memptr = sparse_lift(&smt->entries[i], memptr);
    smt->offsets[i] = smt->size / smt->num_tbls * i;
  }
  return memptr;
}
