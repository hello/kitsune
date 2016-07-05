#ifndef HLO_PROTO_TOOLS_H
#define HLO_PROTO_TOOLS_H
#include "pb.h"
#include "hlo_async.h"
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"
#include "streaming.pb.h"
#include "stdbool.h"

hlo_future_t * MorpheusCommand_from_buffer(void * buf, size_t size);
hlo_future_t * buffer_from_MorpheusCommand(MorpheusCommand * src);

#include "hlo_stream.h"
hlo_stream_t * hlo_hmac_stream( hlo_stream_t * base, uint8_t * key, size_t key_len );
void get_hmac( uint8_t * hmac, hlo_stream_t * stream );
int Cmd_testhmac(int argc, char *argv[]);

int hlo_pb_encode( hlo_stream_t * stream,const pb_field_t * fields, void * structdata );
int hlo_pb_decode( hlo_stream_t * stream,const pb_field_t * fields, void * structdata );

bool hlo_output_pb_wto( Preamble_pb_type hlo_pb_type, const pb_field_t * fields, void * structdata, TickType_t to );
bool hlo_output_pb( Preamble_pb_type hlo_pb_type, const pb_field_t * fields, void * structdata );
void hlo_prep_for_pb(int type);

void hlo_init_pbstream();

#if 0
int Cmd_pbstr(int argc, char *argv[]);
#endif

#endif
