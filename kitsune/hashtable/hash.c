#include "hash.h"

//Macros isolate the traversal logic - here a quadratic probe is used.
#define HASH_ITERATE(name, table, key, key_t, block)\
  hash_bucket bucket;\
  int idx = table->hash_func(key) & (table->bucket_size-1);\
  unsigned int p = 2;\
  while( sparse_multi_get(&table->sparse_table, &bucket, idx) ) {\
    block;\
    p = (p*p + p)/2;\
    idx=(idx+p) & (table->bucket_size-1);\
  } 
#define HASH_LOOKUP(name, table, key, key_t, block)\
  HASH_ITERATE(name, table, key, key_t, if( table->eq_func(bucket.key, key) ) { block; return TRUE; } );\
  return FALSE;

void hash_init(hash_table* table, hash_func_type hash_func, eq_func_type eq_func ) {
  table->bucket_size = HASH_MIN_BUCKETS;
  table->filled_buckets = 0;
  sparse_multi_init(&table->sparse_table, table->bucket_size, sizeof(hash_bucket) );
  table->eq_func = eq_func;
  table->hash_func = hash_func;
}
void hash_dispose(hash_table* table) {
  sparse_multi_dispose(&table->sparse_table);
}
unsigned int hash_memory_used(hash_table * table) {
  return sizeof(table->bucket_size) + sizeof(table->filled_buckets) + sparse_multi_memory_used(&table->sparse_table);
}
unsigned char * hash_dump(hash_table* table, unsigned char * memptr) {
  DUMP_ADVANCE(table->bucket_size, memptr);
  DUMP_ADVANCE(table->filled_buckets, memptr);
  return sparse_multi_dump(&table->sparse_table, memptr);
}
unsigned char * hash_lift(hash_table* table, unsigned char * memptr, hash_func_type hash_func, eq_func_type eq_func) {
  LIFT_ADVANCE(table->bucket_size, memptr);
  LIFT_ADVANCE(table->filled_buckets, memptr);
  table->eq_func = eq_func;
  table->hash_func = hash_func;
  return sparse_multi_lift(&table->sparse_table, memptr);
}
void hash_add(hash_table* table, key_t key, val_t value);
void hash_resize_helper(sparse_table_entry const* entry, void * new_table ) {
  hash_add((hash_table*)new_table, ((hash_bucket*)entry)->key, ((hash_bucket*)entry)->value);
}
void hash_resize(hash_table* table, unsigned int size) {
  hash_table new_table;
  new_table.hash_func = table->hash_func;
  new_table.eq_func = table->eq_func;
  new_table.bucket_size =  size;
  new_table.filled_buckets = 0;
  sparse_multi_init(&new_table.sparse_table, size, sizeof(hash_bucket) );
  sparse_multi_apply_eat( &table->sparse_table, hash_resize_helper, &new_table );
  hash_dispose(table);
  memcpy(table, &new_table, sizeof(hash_table));
}
boolean hash_remove(hash_table* table, key_t key) {
  HASH_LOOKUP(name, table, key, key_t,
    sparse_multi_remove(&table->sparse_table, idx);
    if( ((float)--table->filled_buckets+2) / (float) table->bucket_size < 0.2f )
        hash_resize(table, table->bucket_size / 2); );
}
boolean hash_lookup(hash_table* table, key_t key, val_t value) {
  HASH_LOOKUP(name, table, key, key_t, memcpy( value, bucket.value, sizeof(void*) ) );
}
void hash_add(hash_table* table, key_t key, val_t value) {
  hash_bucket in_bucket = {key, value};
  HASH_ITERATE(name, table, key, key_t,
  if( table->eq_func(bucket.key, key) ) {
    sparse_multi_update(&table->sparse_table, &in_bucket, idx);
    return;
  } );
  
  sparse_multi_update(&table->sparse_table, &in_bucket, idx);
  if( ((float)++table->filled_buckets+2) / (float) table->bucket_size > 0.6f )
    hash_resize(table, table->bucket_size * 2);
}
void hash_apply(hash_table* table, boolean(*fp)(sparse_table_entry const*, void * ), void * data ) {
  sparse_multi_apply( &table->sparse_table, fp, data );
}
