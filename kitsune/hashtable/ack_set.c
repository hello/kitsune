#include "ack_set.h"
#include "serialization_macros.h"

void ack_set_init(ack_set *st, int size ) {
  memset(st, 0, sizeof(*st));
  bit_array_init(&st->presence, size);
  st->allocated = size;
  st->entries = (unsigned char*)pvPortRealloc(st->entries, st->allocated*sizeof(ack_set_entry));
}
void ack_set_dispose(ack_set *st) {
  bit_array_dispose(&st->presence);
  FREE_AND_NULL(st->entries);
}
void ack_set_remove(ack_set *st, uint64_t idx) {
  idx -= st->starting_idx;
  int pos;
  if( !bit_array_get(&st->presence, idx) )
    return;
  pos = bit_array_count(&st->presence, idx);
  bit_array_put(&st->presence, idx, false);
  memmove((void*)(st->entries + pos * sizeof(ack_set_entry)),
          (void*)(st->entries + ( pos + 1 ) * sizeof(ack_set_entry)),
          (st->size-- - pos)*sizeof(ack_set_entry));

  if( st->size ) {
	  int lz = bit_array_clz(&st->presence );
      if( lz>8 ) {
        lz/=8;
        bit_array_rshift_bytes( &st->presence, lz );
        st->starting_idx += lz*8;
      }
  } else {
	  st->starting_idx = 0;
  }
}
bool ack_set_get(ack_set *st, ack_set_entry value, uint64_t idx) {
  idx -= st->starting_idx;
  if( !bit_array_get(&st->presence, idx) )
    return false;
  memcpy( &value, (void*)(st->entries + bit_array_count(&st->presence, idx) * sizeof(ack_set_entry)), sizeof(ack_set_entry) );
  return true;
}
void ack_set_update(ack_set *st, ack_set_entry entry, uint64_t idx ) {
  if( st->starting_idx == 0 ) {
    st->starting_idx = idx;
  }
  idx -= st->starting_idx;
  if( 1+st->size == st->allocated ) { //pop eldest
    ack_set_remove( st, st->starting_idx+bit_array_clz(&st->presence) );
  }
  int pos = bit_array_count(&st->presence, idx);
  if( bit_array_get(&st->presence, idx) ) {
    memcpy( (void*)(st->entries + pos * sizeof(ack_set_entry)), &entry, sizeof(ack_set_entry) );
  } else {
    bit_array_put(&st->presence, idx, true);
    st->size++;
    memmove( (void*)(st->entries + ( pos + 1 ) * sizeof(ack_set_entry)),
             (void*)(st->entries + pos * sizeof(ack_set_entry)) , (st->allocated-pos-1)*sizeof(ack_set_entry));
    memcpy( (void*)(st->entries + pos * sizeof(ack_set_entry)), &entry, sizeof(ack_set_entry) );
  }
}
void ack_set_apply(ack_set *st, bool(*fp)(ack_set_entry const*, void * ), void * data ) {
  int i;
  for(i=st->size-1;i>=0;--i)
    if(!fp( (ack_set_entry const*)(st->entries + i * sizeof(ack_set_entry)) ,data))
      return;
}
ack_set_entry* ack_set_yield(ack_set *st ) {
  if( st->size == 0 ) return NULL;
  if( st->y == 0 ) st->y = st->size;
  return (ack_set_entry*)(st->entries + --st->y * sizeof(ack_set_entry));
}

