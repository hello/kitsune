/* Automatically generated nanopb constant definitions */
/* Generated by nanopb-0.3.1 at Wed Oct 22 18:02:12 2014. */

#include "morpheus_ble.pb.h"

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif



const pb_field_t MorpheusCommand_fields[14] = {
    PB_FIELD(  1, INT32   , REQUIRED, STATIC  , FIRST, MorpheusCommand, version, version, 0),
    PB_FIELD(  2, ENUM    , REQUIRED, STATIC  , OTHER, MorpheusCommand, type, version, 0),
    PB_FIELD(  3, STRING  , OPTIONAL, CALLBACK, OTHER, MorpheusCommand, deviceId, type, 0),
    PB_FIELD(  4, STRING  , OPTIONAL, CALLBACK, OTHER, MorpheusCommand, accountId, deviceId, 0),
    PB_FIELD(  5, ENUM    , OPTIONAL, STATIC  , OTHER, MorpheusCommand, error, accountId, 0),
    PB_FIELD(  6, STRING  , OPTIONAL, CALLBACK, OTHER, MorpheusCommand, wifiName, error, 0),
    PB_FIELD(  7, STRING  , OPTIONAL, CALLBACK, OTHER, MorpheusCommand, wifiSSID, wifiName, 0),
    PB_FIELD(  8, STRING  , OPTIONAL, CALLBACK, OTHER, MorpheusCommand, wifiPassword, wifiSSID, 0),
    PB_FIELD(  9, INT32   , OPTIONAL, STATIC  , OTHER, MorpheusCommand, batteryLevel, wifiPassword, 0),
    PB_FIELD( 10, INT32   , OPTIONAL, STATIC  , OTHER, MorpheusCommand, uptime, batteryLevel, 0),
    PB_FIELD( 11, INT32   , OPTIONAL, STATIC  , OTHER, MorpheusCommand, motionData, uptime, 0),
    PB_FIELD( 12, BYTES   , OPTIONAL, CALLBACK, OTHER, MorpheusCommand, motionDataEntrypted, motionData, 0),
    PB_FIELD( 13, INT32   , OPTIONAL, STATIC  , OTHER, MorpheusCommand, firmwareVersion, motionDataEntrypted, 0),
    PB_LAST_FIELD
};


