#ifndef __BLE_COMMAND_H__
#define __BLE_COMMAND_H__

#include <stdlib.h>
#include <stdbool.h>


#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

#define MAX_STRING_LEN              2048
#define MAX_WIFI_EP_PER_SCAN        10
#define ANTENNA_CNT                 2

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct array_t
{
    uint8_t* buffer;
    size_t length;
} array_data;

void on_morpheus_protobuf_arrival(uint8_t* protobuf, size_t len);
bool ble_send_protobuf(MorpheusCommand* command);
bool ble_reply_protobuf_error(ErrorType error_type);
void ble_proto_free_command(MorpheusCommand* command);
void ble_proto_assign_encode_funcs(MorpheusCommand* command);
void ble_proto_assign_decode_funcs(MorpheusCommand* command);

void ble_proto_get_morpheus_command(pb_field_t ** fields, void ** structdata);
void ble_proto_free_morpheus_command(void * structdata);

int Cmd_factory_reset(int argc, char* argv[]);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __BLE_COMMAND_H__
