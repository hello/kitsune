#include "sparse_table.h"

void sparse_init(sparse_table *st, int size, int entry_sz) {
  bit_array_init(&st->presence, size);
  st->entries = NULL;
  st->allocated = st->size = 0;
  st->entry_sz = entry_sz;
}
void sparse_dispose(sparse_table *st) {
  bit_array_dispose(&st->presence);
  FREE_AND_NULL(st->entries);
}
unsigned int sparse_memory_used(sparse_table * st) {
  return sizeof(st->size) + st->size*st->entry_sz + bit_array_memory_used(&st->presence);
}
unsigned char * sparse_dump(sparse_table *st, unsigned char * memptr) {
  DUMP_ADVANCE(st->size, memptr);
  DUMP_ADVANCE(st->entry_sz, memptr);
  memptr=bit_array_dump(&st->presence, memptr);
  memcpy(memptr, st->entries, st->size*st->entry_sz);
  return memptr+st->size*st->entry_sz;
}
unsigned char * sparse_lift(sparse_table *st, unsigned char * memptr) {
  LIFT_ADVANCE(st->size, memptr);
  LIFT_ADVANCE(st->entry_sz, memptr);
  st->allocated = st->size;
  memptr=bit_array_lift(&st->presence, memptr);
  st->entries = (unsigned char*)pvPortMalloc(st->size*st->entry_sz);
  memcpy(st->entries, memptr, st->size*st->entry_sz);
  return memptr+st->size*st->entry_sz;
}
void sparse_remove(sparse_table *st, int idx) {
  int pos;
  if( !bit_array_get(&st->presence, idx) )
    return;
  pos = bit_array_count(&st->presence, idx);
  bit_array_put(&st->presence, idx, FALSE);
  memmove((void*)(st->entries + pos * st->entry_sz),
          (void*)(st->entries + ( pos + 1 ) * st->entry_sz),
          (st->size-- - pos - 1)*st->entry_sz);
  st->entries = (unsigned char*)pvPortRealloc(st->entries, --st->allocated*st->entry_sz);
}
boolean sparse_get(sparse_table *st, sparse_table_entry value, int idx) {
  if( !bit_array_get(&st->presence, idx) )
    return FALSE;
  memcpy( value, (void*)(st->entries + bit_array_count(&st->presence, idx) * st->entry_sz), st->entry_sz );
  return TRUE;
}
void sparse_update(sparse_table *st, sparse_table_entry entry, int idx ) {
  int pos = bit_array_count(&st->presence, idx);
  if( bit_array_get(&st->presence, idx) ) {
    memcpy( (void*)(st->entries + pos * st->entry_sz), entry, st->entry_sz );
  } else {
    bit_array_put(&st->presence, idx, TRUE);
    if( ++st->size > st->allocated ) {
      st->entries = (unsigned char*)pvPortRealloc(st->entries, st->size*st->entry_sz);
      ++st->allocated;
    }
    memmove( (void*)(st->entries + ( pos + 1 ) * st->entry_sz),
             (void*)(st->entries + pos * st->entry_sz) , (st->size-pos-1)*st->entry_sz);
    memcpy( (void*)(st->entries + pos * st->entry_sz), entry, st->entry_sz );
  }
}
void sparse_apply(sparse_table *st, boolean(*fp)(sparse_table_entry const*, void * ), void * data ) {
  int i;
  for(i=st->size-1;i>=0;--i)
    if(!fp( (sparse_table_entry const*)(st->entries + i * st->entry_sz) ,data))
      return;
}
void sparse_apply_eat(sparse_table *st, void(*fp)(sparse_table_entry const*, void * ), void * data ) {
  int i;
  for(i=st->size-1;i>=0;--i) {
    fp((sparse_table_entry const*)(st->entries + i * st->entry_sz),data);
    st->entries = (unsigned char*)pvPortRealloc(st->entries, --st->size*st->entry_sz);
    --st->allocated;
  }
}
