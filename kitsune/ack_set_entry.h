#ifndef _ACK_SET_ENTRY_H
#define _ACK_SET_ENTRY_H

#include "pb.h"
#include "streaming.pb.h"

typedef struct {
	const pb_field_t * fields;
	void * structdata;
	Preamble_pb_type hlo_pb_type;
	uint64_t id;
} pb_msg;
#define ack_set_entry pb_msg


#endif
