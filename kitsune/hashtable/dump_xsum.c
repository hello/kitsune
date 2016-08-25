#include "dump_xsum.h"
#include "hash_functions.h"

#include "kit_assert.h"

DUMP_XSUM_TYPE dump_xsum_compute( unsigned char * data, unsigned int size ) {
  return (DUMP_XSUM_TYPE)hash((char*)data, size, 0xA5239b5);
}
void dump_xsum_apply( unsigned char * data, unsigned int size ) {
  *(DUMP_XSUM_TYPE*)(data+size) = dump_xsum_compute(data, size);
}
void dump_xsum_verify( unsigned char * data, unsigned int size ) {
  assert(*(DUMP_XSUM_TYPE*)(data+size)==dump_xsum_compute(data, size));
}
