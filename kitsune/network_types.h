#ifndef _NETWORKTYPES_H_
#define _NETWORKTYPES_H_

#include <pb.h>
#include <stdint.h>

typedef uint32_t (* network_encode_callback_t)(pb_ostream_t * stream, const void * encode_data);

typedef uint32_t (* network_decode_callback_t)(pb_istream_t * stream, void * decode_target);

typedef void (*network_prep_callback_t)(void * data);

typedef struct {
	const void * encodedata;
	const pb_field_t * fields;
} network_encode_data_t;

typedef struct {
	void * decodedata;
	const pb_field_t * fields;
} network_decode_data_t;

#endif // _NETWORKTYPES_H_
