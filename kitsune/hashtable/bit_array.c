#include "bit_array.h"
#include "serialization_macros.h"

#include <FreeRTOS.h> /* realloc, free, malloc */
#include <string.h> /* memset, memcpy */

void bit_array_resize(bit_array* ba, int size) {
  if( size % 8 != 0 )
    size+=8;
  size/=8;
  ba->bits = (unsigned char*)pvPortRealloc(ba->bits, size);
  if( size > ba->size/8 )
    memset(ba->bits+ba->size/8, 0, size - ba->size/8);
  ba->size = size*8;
}
void bit_array_init(bit_array* ba, int size) {
  ba->bits = NULL;
  ba->size = 0;
  bit_array_resize(ba, size);
}
void bit_array_dispose(bit_array* ba) {
  FREE_AND_NULL(ba->bits);
}
bool bit_array_get(bit_array* ba, int idx) {
  if( idx >= ba->size ) return false;
  return ( ba->bits[idx/8] & 1<<(idx % 8) ) != 0;
}
void bit_array_put(bit_array* ba, int idx, bool val) {
  int shift;
  if( idx/8 >= ba->size )
    bit_array_resize(ba, idx/8);
  shift = idx % 8;
  if( val )
    ba->bits[idx/8] |= 1<<shift;
  else
    ba->bits[idx/8] &= ~(1<<shift);
}
int bit_array_clz(bit_array* ba ) {
  unsigned int total=0;
  int i=0;
  int pos = 0;
  int * p;
  p = (int*) ba->bits;
  for(; i<(ba->size/32); i+=4) {
	if(*p++) {
		break;
	} else {
        total += 32;
    }
  }
  for(; i<(ba->size/8); i++) {
	if(ba->bits[i]) {
		break;
	} else {
        total += 8;
    }
  }
  pos = i;
  for(i=0;i<8;++i) { /*count last 8 bits*/
	if( ba->bits[pos] & 1<<i ) {
		break;
    } else {
    	++total;
    }
  }
  return total;
}
void bit_array_rshift_bytes(bit_array* ba, int s) {
  memmove( ba->bits, ba->bits + s, ba->size/8 - s );
  ba->size -= s*8;
  bit_array_resize(ba, ba->size);
}
static unsigned int popcnt_int(int v) {
  v = v - ((v >> 1) & 0x55555555);
  v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
  return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}
static unsigned int popcnt_char(char b) {
  unsigned int count = 0;
  for (count = 0; b != 0; count++)
    b &= b - 1;
  return count;
}
unsigned int bit_array_count(bit_array* ba, int idx) {
  unsigned int total=0;
  int i=0;  
  int * p;
  if( idx > ba->size ) { bit_array_resize(ba, idx); }
  p = (int*) ba->bits;
  for(; i < idx/32 && i<ba->size/32; i+=4) {
    total+=popcnt_int(*p++);
  }
  for(; i < idx/8 && i<ba->size/8; i++) {
    total+=popcnt_char((char)ba->bits[i]);
  }
  if(i<ba->size/8) {
    for(i=0;i<idx%8;++i) { /*count last 8 bits*/
      if( ba->bits[idx/8] & 1<<i ) {
        ++total;
  } } }
  return total;
}
unsigned int bit_array_memory_used(bit_array * ba) {
  return sizeof(ba->size) + ba->size/8;
}
unsigned char* bit_array_dump( bit_array* ba, unsigned char * memptr ) {
  DUMP_ADVANCE(ba->size, memptr);
  memcpy(memptr, ba->bits, ba->size/8);
  return memptr+ba->size/8;
}
unsigned char* bit_array_lift( bit_array* ba, unsigned char * memptr ) {
  LIFT_ADVANCE(ba->size, memptr);
  ba->bits = (unsigned char*)pvPortMalloc(ba->size/8);
  memcpy(ba->bits, memptr, ba->size/8);
  return memptr+ba->size/8;
}
