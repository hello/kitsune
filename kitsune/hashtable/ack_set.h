#ifndef _ACK_SET_H
#define _ACK_SET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bit_array.h"
#include "stdint.h"

#include <FreeRTOS.h> /* realloc, free, malloc */
#include <string.h> /* memcpy, memmove */

typedef struct {
  bit_array presence;
  unsigned char * entries;
  unsigned int size;
  unsigned int allocated;
  uint64_t starting_idx;
  int y;
} ack_set;

#include "ack_set_entry.h"

void ack_set_init(ack_set *st, int size);
void ack_set_dispose(ack_set *st);
void ack_set_remove(ack_set *st, uint64_t idx);
bool ack_set_get(ack_set *st,  ack_set_entry value, uint64_t idx);
void ack_set_update(ack_set *st, ack_set_entry entry, uint64_t idx );
void ack_set_apply(ack_set *st, bool(*fp)(ack_set_entry const*, void * ), void * data );
ack_set_entry * ack_set_yield(ack_set *st);

#ifdef __cplusplus
}
#endif

#endif
