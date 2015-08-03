#ifndef __PROTO_UTILS__
#define __PROTO_UTILS__
#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/periodic.pb.h"
#include "protobuf/morpheus_ble.pb.h"
#include "hlo_async.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pill_data * pills;
    int num_pills;
} pilldata_to_encode;

typedef struct {
    periodic_data * data;
    int num_data;
    hlo_future_t * scan_result;
} periodic_data_to_encode;


bool encode_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool encode_all_periodic_data (pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool encode_all_pills (pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool encode_scanned_ssid (pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
void pack_batched_periodic_data(batched_periodic_data* batched, periodic_data_to_encode* encode_wrapper);
bool _encode_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

#ifdef __cplusplus
}
#endif

#endif
