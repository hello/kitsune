#ifndef __BLE_PROTO_H__
#define __BLE_PROTO_H__

#include <stdlib.h>


#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

#define PROTOBUF_VERSION            0
#define MAX_WIFI_EP_PER_SCAN        100

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

void on_ble_protobuf_command(MorpheusCommand* command);
bool set_wifi(const char* ssid, char* password);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __BLE_PROTO_H__
