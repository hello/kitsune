#ifndef HLO_PROTO_TOOLS_H
#define HLO_PROTO_TOOLS_H
#include "hlo_async.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

hlo_future_t * MorpheusCommand_from_buffer(void * buf, size_t size);
hlo_future_t * buffer_from_MorpheusCommand(MorpheusCommand * src);

#include "hlo_stream.h"
/**
 * wrapper class for signing a stream
 * base stream is managed by this
 * signature is appended onto the data
 * WRITE ONLY
 */
hlo_stream_t * sign_stream(const hlo_stream_t * base);
/**
 * wrapper class for verifying a stream
 * base stream is managed by this
 * signature is prepended onto the data
 * READ_ONLY
 */
hlo_stream_t * verify_stream(const hlo_stream_t * base);
#endif
