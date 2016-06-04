#ifndef HLO_PROTO_TOOLS_H
#define HLO_PROTO_TOOLS_H
#include "pb.h"
#include "hlo_async.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

hlo_future_t * MorpheusCommand_from_buffer(void * buf, size_t size);
hlo_future_t * buffer_from_MorpheusCommand(MorpheusCommand * src);

#include "hlo_stream.h"
hlo_stream_t * hlo_hmac_stream( hlo_stream_t * base, uint8_t * key, size_t key_len );
void get_hmac( uint8_t * hmac, hlo_stream_t * stream );

int hlo_pb_encode( hlo_stream_t * stream,const pb_field_t * fields, void * structdata );
int hlo_pb_decode( hlo_stream_t * stream,const pb_field_t * fields, void * structdata );


#endif
