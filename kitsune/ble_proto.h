#ifndef __BLE_PROTO_H__
#define __BLE_PROTO_H__

#include <stdlib.h>


#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

#include "wifi_cmd.h"
#include "kitsune_version.h"

#define PROTOBUF_VERSION            0
#define FIRMWARE_VERSION_INTERNAL   KIT_VER  //

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
void ble_proto_start_hold();
void ble_proto_end_hold();
bool on_ble_protobuf_command(MorpheusCommand* command);
void ble_proto_led_init();

void ble_proto_led_busy_mode(uint8_t a, uint8_t r, uint8_t g, uint8_t b, int delay);
void ble_proto_led_fade_in_trippy();
void ble_proto_led_fade_out(bool operation_result);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __BLE_PROTO_H__
