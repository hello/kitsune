#ifndef HLO_PROTO_TOOLS_H
#define HLO_PROTO_TOOLS_H
#include "hlo_async.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

hlo_future_t * MorpheusCommand_from_buffer(void * buf, size_t size);
hlo_future_t * buffer_from_MorpheusCommand(MorpheusCommand * src);

#include "hlo_stream.h"
bool hlo_decode_varint(hlo_stream_t * in, uint64_t * dest);
#endif
