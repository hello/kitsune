/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.1 at Tue Nov  4 12:28:29 2014. */

#ifndef PB_SYNCRESPONSE_PB_H_INCLUDED
#define PB_SYNCRESPONSE_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _SyncResponse_RoomConditions {
    SyncResponse_RoomConditions_IDEAL = 1,
    SyncResponse_RoomConditions_WARNING = 2,
    SyncResponse_RoomConditions_ALERT = 3
} SyncResponse_RoomConditions;

typedef enum _SyncResponse_PairingAction_ActionType {
    SyncResponse_PairingAction_ActionType_PAIR = 0,
    SyncResponse_PairingAction_ActionType_UNPAIR = 1
} SyncResponse_PairingAction_ActionType;

/* Struct definitions */
typedef struct _SyncResponse_Alarm {
    bool has_start_time;
    int32_t start_time;
    bool has_end_time;
    int32_t end_time;
    bool has_ringtone_id;
    int32_t ringtone_id;
    bool has_ring_offset_from_now_in_second;
    int32_t ring_offset_from_now_in_second;
    bool has_ring_duration_in_second;
    int32_t ring_duration_in_second;
} SyncResponse_Alarm;

typedef struct _SyncResponse_FlashAction_LEDAction {
    bool has_color;
    int32_t color;
    bool has_start_time;
    int32_t start_time;
    bool has_end_time;
    int32_t end_time;
} SyncResponse_FlashAction_LEDAction;

typedef struct _SyncResponse_PairingAction {
    pb_callback_t ssid;
    bool has_type;
    SyncResponse_PairingAction_ActionType type;
} SyncResponse_PairingAction;

typedef struct _SyncResponse_WhiteNoise {
    bool has_start_time;
    int32_t start_time;
    bool has_end_time;
    int32_t end_time;
    bool has_sound_id;
    int32_t sound_id;
} SyncResponse_WhiteNoise;

typedef struct _SyncResponse_FlashAction {
    bool has_led_1;
    SyncResponse_FlashAction_LEDAction led_1;
    bool has_led_2;
    SyncResponse_FlashAction_LEDAction led_2;
    bool has_led_3;
    SyncResponse_FlashAction_LEDAction led_3;
    bool has_led_4;
    SyncResponse_FlashAction_LEDAction led_4;
    bool has_led_5;
    SyncResponse_FlashAction_LEDAction led_5;
} SyncResponse_FlashAction;

typedef struct _SyncResponse {
    bool has_upload_cycle;
    int32_t upload_cycle;
    bool has_sync_cycle;
    int32_t sync_cycle;
    bool has_acc_scan_cyle;
    int32_t acc_scan_cyle;
    bool has_acc_sampling_interval;
    int32_t acc_sampling_interval;
    bool has_device_sampling_interval;
    int32_t device_sampling_interval;
    bool has_alarm;
    SyncResponse_Alarm alarm;
    bool has_pairing_action;
    SyncResponse_PairingAction pairing_action;
    bool has_white_noise;
    SyncResponse_WhiteNoise white_noise;
    bool has_flash_action;
    SyncResponse_FlashAction flash_action;
    bool has_reset_device;
    bool reset_device;
    bool has_room_conditions;
    SyncResponse_RoomConditions room_conditions;
} SyncResponse;

/* Default values for struct fields */
extern const int32_t SyncResponse_Alarm_ringtone_id_default;

/* Initializer values for message structs */
#define SyncResponse_init_default                {false, 0, false, 0, false, 0, false, 0, false, 0, false, SyncResponse_Alarm_init_default, false, SyncResponse_PairingAction_init_default, false, SyncResponse_WhiteNoise_init_default, false, SyncResponse_FlashAction_init_default, false, 0, false, (SyncResponse_RoomConditions)0}
#define SyncResponse_Alarm_init_default          {false, 0, false, 0, false, 1, false, 0, false, 0}
#define SyncResponse_PairingAction_init_default  {{{NULL}, NULL}, false, (SyncResponse_PairingAction_ActionType)0}
#define SyncResponse_WhiteNoise_init_default     {false, 0, false, 0, false, 0}
#define SyncResponse_FlashAction_init_default    {false, SyncResponse_FlashAction_LEDAction_init_default, false, SyncResponse_FlashAction_LEDAction_init_default, false, SyncResponse_FlashAction_LEDAction_init_default, false, SyncResponse_FlashAction_LEDAction_init_default, false, SyncResponse_FlashAction_LEDAction_init_default}
#define SyncResponse_FlashAction_LEDAction_init_default {false, 0, false, 0, false, 0}
#define SyncResponse_init_zero                   {false, 0, false, 0, false, 0, false, 0, false, 0, false, SyncResponse_Alarm_init_zero, false, SyncResponse_PairingAction_init_zero, false, SyncResponse_WhiteNoise_init_zero, false, SyncResponse_FlashAction_init_zero, false, 0, false, (SyncResponse_RoomConditions)0}
#define SyncResponse_Alarm_init_zero             {false, 0, false, 0, false, 0, false, 0, false, 0}
#define SyncResponse_PairingAction_init_zero     {{{NULL}, NULL}, false, (SyncResponse_PairingAction_ActionType)0}
#define SyncResponse_WhiteNoise_init_zero        {false, 0, false, 0, false, 0}
#define SyncResponse_FlashAction_init_zero       {false, SyncResponse_FlashAction_LEDAction_init_zero, false, SyncResponse_FlashAction_LEDAction_init_zero, false, SyncResponse_FlashAction_LEDAction_init_zero, false, SyncResponse_FlashAction_LEDAction_init_zero, false, SyncResponse_FlashAction_LEDAction_init_zero}
#define SyncResponse_FlashAction_LEDAction_init_zero {false, 0, false, 0, false, 0}

/* Field tags (for use in manual encoding/decoding) */
#define SyncResponse_Alarm_start_time_tag        1
#define SyncResponse_Alarm_end_time_tag          2
#define SyncResponse_Alarm_ringtone_id_tag       3
#define SyncResponse_Alarm_ring_offset_from_now_in_second_tag 4
#define SyncResponse_Alarm_ring_duration_in_second_tag 5
#define SyncResponse_FlashAction_LEDAction_color_tag 1
#define SyncResponse_FlashAction_LEDAction_start_time_tag 2
#define SyncResponse_FlashAction_LEDAction_end_time_tag 3
#define SyncResponse_PairingAction_ssid_tag      1
#define SyncResponse_PairingAction_type_tag      2
#define SyncResponse_WhiteNoise_start_time_tag   1
#define SyncResponse_WhiteNoise_end_time_tag     2
#define SyncResponse_WhiteNoise_sound_id_tag     3
#define SyncResponse_FlashAction_led_1_tag       1
#define SyncResponse_FlashAction_led_2_tag       2
#define SyncResponse_FlashAction_led_3_tag       3
#define SyncResponse_FlashAction_led_4_tag       4
#define SyncResponse_FlashAction_led_5_tag       5
#define SyncResponse_upload_cycle_tag            1
#define SyncResponse_sync_cycle_tag              2
#define SyncResponse_acc_scan_cyle_tag           3
#define SyncResponse_acc_sampling_interval_tag   4
#define SyncResponse_device_sampling_interval_tag 5
#define SyncResponse_alarm_tag                   6
#define SyncResponse_pairing_action_tag          7
#define SyncResponse_white_noise_tag             8
#define SyncResponse_flash_action_tag            9
#define SyncResponse_reset_device_tag            10
#define SyncResponse_room_conditions_tag         12

/* Struct field encoding specification for nanopb */
extern const pb_field_t SyncResponse_fields[12];
extern const pb_field_t SyncResponse_Alarm_fields[6];
extern const pb_field_t SyncResponse_PairingAction_fields[3];
extern const pb_field_t SyncResponse_WhiteNoise_fields[4];
extern const pb_field_t SyncResponse_FlashAction_fields[6];
extern const pb_field_t SyncResponse_FlashAction_LEDAction_fields[4];

/* Maximum encoded size of messages (where known) */
#define SyncResponse_Alarm_size                  55
#define SyncResponse_WhiteNoise_size             33
#define SyncResponse_FlashAction_size            175
#define SyncResponse_FlashAction_LEDAction_size  33

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
