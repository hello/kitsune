#ifndef HLO_PROTO_TOOLS_H
#define HLO_PROTO_TOOLS_H
#include "hlo_async.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

int MorpheusCommand_from_buffer(MorpheusCommand * dst, void * buf, size_t size);
void * buffer_from_MorpheusCommand(MorpheusCommand * src, int * out_size);

#endif
