#include "circular_buffer.h"
#include "string.h"

#define ltyp unsigned short /* adjust to allow for higher max string length */
#define WRAP_MAGIC (ltyp)(-1)

void circular_buffer_init( circular_buffer * csb, void * memory, unsigned int size ) {
  csb->top = csb->begin = (unsigned char *)memory;
  csb->bottom = csb->top + size - sizeof(ltyp);
  csb->end = csb->begin + size;
  *(ltyp*)csb->bottom = WRAP_MAGIC;
}
boolean circular_buffer_queue( circular_buffer * csb, void * memory, unsigned int size ) {
  if( ( csb->top < csb->bottom && csb->top + size + sizeof(ltyp) > csb->bottom ) ||
      ( csb->top > csb->bottom && csb->begin + size + sizeof(ltyp) > csb->bottom ) ||
      ( size > (csb->end - csb->begin) - sizeof(ltyp) ) ||
      ( size > WRAP_MAGIC ) ) {
      return FALSE; /* overflow, too much data, or you need to make ltyp bigger */
    }
  if( csb->top + size + sizeof(ltyp) > csb->end ) {
    if( csb->top + sizeof(ltyp) < csb->end ) {
      *(ltyp*)csb->top = WRAP_MAGIC;
    }
    csb->top = csb->begin;
  }
  *(ltyp*)csb->top = (ltyp)size;
  csb->top += sizeof(ltyp);
  memcpy( (void*)csb->top, memory, size );
  csb->top += size;
  return TRUE;
}
boolean circular_buffer_deque( circular_buffer * csb, void * memory, unsigned int * size ) {
  if( csb->bottom == csb->top || csb->top == csb->begin ) {
    return FALSE; /* underflow or empty */
  }
  if( *(ltyp*)csb->bottom == WRAP_MAGIC ) {
    csb->bottom = csb->begin;
    }
  *size = *(ltyp*)csb->bottom;
  csb->bottom += sizeof(ltyp);
  memcpy( memory, csb->bottom, *size );
  csb->bottom += *size;
  if( csb->bottom + sizeof(ltyp) > csb->end ) {
    csb->bottom = csb->begin;
    }
  return TRUE;
}
