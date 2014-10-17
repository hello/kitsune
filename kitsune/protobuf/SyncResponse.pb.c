/* Automatically generated nanopb constant definitions */
/* Generated by nanopb-0.2.8 at Fri Oct 17 11:38:55 2014. */

#include "SyncResponse.pb.h"

const int32_t SyncResponse_Alarm_ringtone_id_default = 1;


const pb_field_t SyncResponse_fields[11] = {
    PB_FIELD2(  1, INT32   , OPTIONAL, STATIC  , FIRST, SyncResponse, upload_cycle, upload_cycle, 0),
    PB_FIELD2(  2, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse, sync_cycle, upload_cycle, 0),
    PB_FIELD2(  3, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse, acc_scan_cyle, sync_cycle, 0),
    PB_FIELD2(  4, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse, acc_sampling_interval, acc_scan_cyle, 0),
    PB_FIELD2(  5, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse, device_sampling_interval, acc_sampling_interval, 0),
    PB_FIELD2(  6, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse, alarm, device_sampling_interval, &SyncResponse_Alarm_fields),
    PB_FIELD2(  7, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse, pairing_action, alarm, &SyncResponse_PairingAction_fields),
    PB_FIELD2(  8, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse, white_noise, pairing_action, &SyncResponse_WhiteNoise_fields),
    PB_FIELD2(  9, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse, flash_action, white_noise, &SyncResponse_FlashAction_fields),
    PB_FIELD2( 10, BOOL    , OPTIONAL, STATIC  , OTHER, SyncResponse, reset_device, flash_action, 0),
    PB_LAST_FIELD
};

const pb_field_t SyncResponse_Alarm_fields[4] = {
    PB_FIELD2(  1, INT32   , OPTIONAL, STATIC  , FIRST, SyncResponse_Alarm, start_time, start_time, 0),
    PB_FIELD2(  2, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse_Alarm, end_time, start_time, 0),
    PB_FIELD2(  3, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse_Alarm, ringtone_id, end_time, &SyncResponse_Alarm_ringtone_id_default),
    PB_LAST_FIELD
};

const pb_field_t SyncResponse_PairingAction_fields[3] = {
    PB_FIELD2(  1, STRING  , OPTIONAL, CALLBACK, FIRST, SyncResponse_PairingAction, ssid, ssid, 0),
    PB_FIELD2(  2, ENUM    , OPTIONAL, STATIC  , OTHER, SyncResponse_PairingAction, type, ssid, 0),
    PB_LAST_FIELD
};

const pb_field_t SyncResponse_WhiteNoise_fields[4] = {
    PB_FIELD2(  1, INT32   , OPTIONAL, STATIC  , FIRST, SyncResponse_WhiteNoise, start_time, start_time, 0),
    PB_FIELD2(  2, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse_WhiteNoise, end_time, start_time, 0),
    PB_FIELD2(  3, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse_WhiteNoise, sound_id, end_time, 0),
    PB_LAST_FIELD
};

const pb_field_t SyncResponse_FlashAction_fields[6] = {
    PB_FIELD2(  1, MESSAGE , OPTIONAL, STATIC  , FIRST, SyncResponse_FlashAction, led_1, led_1, &SyncResponse_FlashAction_LEDAction_fields),
    PB_FIELD2(  2, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse_FlashAction, led_2, led_1, &SyncResponse_FlashAction_LEDAction_fields),
    PB_FIELD2(  3, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse_FlashAction, led_3, led_2, &SyncResponse_FlashAction_LEDAction_fields),
    PB_FIELD2(  4, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse_FlashAction, led_4, led_3, &SyncResponse_FlashAction_LEDAction_fields),
    PB_FIELD2(  5, MESSAGE , OPTIONAL, STATIC  , OTHER, SyncResponse_FlashAction, led_5, led_4, &SyncResponse_FlashAction_LEDAction_fields),
    PB_LAST_FIELD
};

const pb_field_t SyncResponse_FlashAction_LEDAction_fields[4] = {
    PB_FIELD2(  1, INT32   , OPTIONAL, STATIC  , FIRST, SyncResponse_FlashAction_LEDAction, color, color, 0),
    PB_FIELD2(  2, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse_FlashAction_LEDAction, start_time, color, 0),
    PB_FIELD2(  3, INT32   , OPTIONAL, STATIC  , OTHER, SyncResponse_FlashAction_LEDAction, end_time, start_time, 0),
    PB_LAST_FIELD
};


/* Check that field information fits in pb_field_t */
#if !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_32BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 * 
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in 8 or 16 bit
 * field descriptors.
 */
STATIC_ASSERT((pb_membersize(SyncResponse, alarm) < 65536 && pb_membersize(SyncResponse, pairing_action) < 65536 && pb_membersize(SyncResponse, white_noise) < 65536 && pb_membersize(SyncResponse, flash_action) < 65536 && pb_membersize(SyncResponse_FlashAction, led_1) < 65536 && pb_membersize(SyncResponse_FlashAction, led_2) < 65536 && pb_membersize(SyncResponse_FlashAction, led_3) < 65536 && pb_membersize(SyncResponse_FlashAction, led_4) < 65536 && pb_membersize(SyncResponse_FlashAction, led_5) < 65536), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_SyncResponse_SyncResponse_Alarm_SyncResponse_PairingAction_SyncResponse_WhiteNoise_SyncResponse_FlashAction_SyncResponse_FlashAction_LEDAction)
#endif

#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_16BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 * 
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in the default
 * 8 bit descriptors.
 */
STATIC_ASSERT((pb_membersize(SyncResponse, alarm) < 256 && pb_membersize(SyncResponse, pairing_action) < 256 && pb_membersize(SyncResponse, white_noise) < 256 && pb_membersize(SyncResponse, flash_action) < 256 && pb_membersize(SyncResponse_FlashAction, led_1) < 256 && pb_membersize(SyncResponse_FlashAction, led_2) < 256 && pb_membersize(SyncResponse_FlashAction, led_3) < 256 && pb_membersize(SyncResponse_FlashAction, led_4) < 256 && pb_membersize(SyncResponse_FlashAction, led_5) < 256), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_SyncResponse_SyncResponse_Alarm_SyncResponse_PairingAction_SyncResponse_WhiteNoise_SyncResponse_FlashAction_SyncResponse_FlashAction_LEDAction)
#endif


