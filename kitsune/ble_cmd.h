#ifndef __BLE_COMMAND_H__
#define __BLE_COMMAND_H__

#include <stdlib.h>


#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/morpheus_ble.pb.h"

#define MAX_STRING_LEN      256

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

void on_morpheus_protobuf_arrival(const char* protobuf, size_t len);
bool send_protobuf_to_ble(const MorpheusCommand* command);
void free_protobuf_command(const MorpheusCommand* command);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __BLE_COMMAND_H__