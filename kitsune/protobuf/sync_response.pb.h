/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.1 at Thu Jul 16 13:11:50 2015. */

#ifndef PB_SYNC_RESPONSE_PB_H_INCLUDED
#define PB_SYNC_RESPONSE_PB_H_INCLUDED
#include <pb.h>

#include "audio_control.pb.h"

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
    SyncResponse_RoomConditions_ALERT = 3,
    SyncResponse_RoomConditions_IDEAL_EXCLUDING_LIGHT = 4
} SyncResponse_RoomConditions;

typedef enum _SyncResponse_PairingAction_ActionType {
    SyncResponse_PairingAction_ActionType_PAIR = 0,
    SyncResponse_PairingAction_ActionType_UNPAIR = 1
} SyncResponse_PairingAction_ActionType;

typedef enum _SyncResponse_LEDAction_LEDActionType {
    SyncResponse_LEDAction_LEDActionType_FADEIO = 0,
    SyncResponse_LEDAction_LEDActionType_GLOW = 1,
    SyncResponse_LEDAction_LEDActionType_THROB = 2,
    SyncResponse_LEDAction_LEDActionType_PULSAR = 3,
    SyncResponse_LEDAction_LEDActionType_DOUBLE = 4,
    SyncResponse_LEDAction_LEDActionType_SIREN = 5,
    SyncResponse_LEDAction_LEDActionType_TRIPPY = 6,
    SyncResponse_LEDAction_LEDActionType_PARTY = 7,
    SyncResponse_LEDAction_LEDActionType_URL = 8
} SyncResponse_LEDAction_LEDActionType;

/* Struct definitions */
typedef struct _SyncResponse_Alarm {
    bool has_start_time;
    uint32_t start_time;
    bool has_end_time;
    uint32_t end_time;
    bool has_ringtone_id;
    int32_t ringtone_id;
    bool has_ring_offset_from_now_in_second;
    int32_t ring_offset_from_now_in_second;
    bool has_ring_duration_in_second;
    int32_t ring_duration_in_second;
    bool has_ringtone_path;
    char ringtone_path[24];
} SyncResponse_Alarm;

typedef PB_BYTES_ARRAY_T(20) SyncResponse_FileDownload_sha1_t;

typedef struct _SyncResponse_FileDownload {
    pb_callback_t host;
    pb_callback_t url;
    pb_callback_t sd_card_filename;
    bool has_copy_to_serial_flash;
    bool copy_to_serial_flash;
    bool has_reset_network_processor;
    bool reset_network_processor;
    bool has_reset_application_processor;
    bool reset_application_processor;
    pb_callback_t serial_flash_filename;
    pb_callback_t serial_flash_path;
    pb_callback_t sd_card_path;
    bool has_sha1;
    SyncResponse_FileDownload_sha1_t sha1;
} SyncResponse_FileDownload;

typedef struct _SyncResponse_FlashAction {
    bool has_red;
    int32_t red;
    bool has_green;
    int32_t green;
    bool has_blue;
    int32_t blue;
    bool has_delay_milliseconds;
    int32_t delay_milliseconds;
    bool has_fade_in;
    bool fade_in;
    bool has_fade_out;
    bool fade_out;
    bool has_rotate;
    bool rotate;
    bool has_alpha;
    int32_t alpha;
} SyncResponse_FlashAction;

typedef struct _SyncResponse_LEDAction {
    bool has_type;
    SyncResponse_LEDAction_LEDActionType type;
    pb_callback_t url;
    bool has_color;
    uint32_t color;
    bool has_alt_color;
    uint32_t alt_color;
    bool has_duration_seconds;
    int32_t duration_seconds;
} SyncResponse_LEDAction;

typedef struct _SyncResponse_PairingAction {
    pb_callback_t ssid;
    bool has_type;
    SyncResponse_PairingAction_ActionType type;
} SyncResponse_PairingAction;

typedef struct _SyncResponse_PillSettings {
    bool has_pill_id;
    char pill_id[17];
    bool has_pill_color;
    uint32_t pill_color;
} SyncResponse_PillSettings;

typedef struct _SyncResponse_WhiteNoise {
    bool has_start_time;
    int32_t start_time;
    bool has_end_time;
    int32_t end_time;
    bool has_sound_id;
    int32_t sound_id;
} SyncResponse_WhiteNoise;

typedef struct _BatchedPillSettings {
    pb_size_t pill_settings_count;
    SyncResponse_PillSettings pill_settings[2];
} BatchedPillSettings;

typedef PB_BYTES_ARRAY_T(6) SyncResponse_mac_t;

typedef struct _SyncResponse {
    bool has_alarm;
    SyncResponse_Alarm alarm;
    bool has_white_noise;
    SyncResponse_WhiteNoise white_noise;
    bool has_reset_device;
    bool reset_device;
    bool has_room_conditions;
    SyncResponse_RoomConditions room_conditions;
    pb_callback_t files;
    bool has_reset_to_factory_fw;
    bool reset_to_factory_fw;
    bool has_audio_control;
    AudioControl audio_control;
    bool has_mac;
    SyncResponse_mac_t mac;
    bool has_batch_size;
    int32_t batch_size;
    pb_callback_t pill_settings;
    bool has_reset_mcu;
    bool reset_mcu;
    bool has_upload_log_level;
    uint32_t upload_log_level;
    bool has_pill_batch_size;
    uint32_t pill_batch_size;
    bool has_ring_time_ack;
    char ring_time_ack[30];
    bool has_room_conditions_lights_off;
    SyncResponse_RoomConditions room_conditions_lights_off;
} SyncResponse;

/* Default values for struct fields */
extern const int32_t SyncResponse_Alarm_ringtone_id_default;

/* Initializer values for message structs */
#define SyncResponse_init_default                {false, SyncResponse_Alarm_init_default, false, SyncResponse_WhiteNoise_init_default, false, 0, false, (SyncResponse_RoomConditions)0, {{NULL}, NULL}, false, 0, false, AudioControl_init_default, false, {0, {0}}, false, 0, {{NULL}, NULL}, false, 0, false, 0, false, 0, false, "", false, (SyncResponse_RoomConditions)0}
#define SyncResponse_FileDownload_init_default   {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, false, 0, false, 0, false, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, false, {0, {0}}}
#define SyncResponse_Alarm_init_default          {false, 0, false, 0, false, 1, false, 0, false, 0, false, ""}
#define SyncResponse_PairingAction_init_default  {{{NULL}, NULL}, false, (SyncResponse_PairingAction_ActionType)0}
#define SyncResponse_WhiteNoise_init_default     {false, 0, false, 0, false, 0}
#define SyncResponse_FlashAction_init_default    {false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0}
#define SyncResponse_LEDAction_init_default      {false, (SyncResponse_LEDAction_LEDActionType)0, {{NULL}, NULL}, false, 0, false, 0, false, 0}
#define SyncResponse_PillSettings_init_default   {false, "", false, 0}
#define BatchedPillSettings_init_default         {0, {SyncResponse_PillSettings_init_default, SyncResponse_PillSettings_init_default}}
#define SyncResponse_init_zero                   {false, SyncResponse_Alarm_init_zero, false, SyncResponse_WhiteNoise_init_zero, false, 0, false, (SyncResponse_RoomConditions)0, {{NULL}, NULL}, false, 0, false, AudioControl_init_zero, false, {0, {0}}, false, 0, {{NULL}, NULL}, false, 0, false, 0, false, 0, false, "", false, (SyncResponse_RoomConditions)0}
#define SyncResponse_FileDownload_init_zero      {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, false, 0, false, 0, false, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, false, {0, {0}}}
#define SyncResponse_Alarm_init_zero             {false, 0, false, 0, false, 0, false, 0, false, 0, false, ""}
#define SyncResponse_PairingAction_init_zero     {{{NULL}, NULL}, false, (SyncResponse_PairingAction_ActionType)0}
#define SyncResponse_WhiteNoise_init_zero        {false, 0, false, 0, false, 0}
#define SyncResponse_FlashAction_init_zero       {false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0}
#define SyncResponse_LEDAction_init_zero         {false, (SyncResponse_LEDAction_LEDActionType)0, {{NULL}, NULL}, false, 0, false, 0, false, 0}
#define SyncResponse_PillSettings_init_zero      {false, "", false, 0}
#define BatchedPillSettings_init_zero            {0, {SyncResponse_PillSettings_init_zero, SyncResponse_PillSettings_init_zero}}

/* Field tags (for use in manual encoding/decoding) */
#define SyncResponse_Alarm_start_time_tag        1
#define SyncResponse_Alarm_end_time_tag          2
#define SyncResponse_Alarm_ringtone_id_tag       3
#define SyncResponse_Alarm_ring_offset_from_now_in_second_tag 4
#define SyncResponse_Alarm_ring_duration_in_second_tag 5
#define SyncResponse_Alarm_ringtone_path_tag     6
#define SyncResponse_FileDownload_host_tag       1
#define SyncResponse_FileDownload_url_tag        2
#define SyncResponse_FileDownload_copy_to_serial_flash_tag 4
#define SyncResponse_FileDownload_reset_network_processor_tag 5
#define SyncResponse_FileDownload_reset_application_processor_tag 6
#define SyncResponse_FileDownload_serial_flash_filename_tag 7
#define SyncResponse_FileDownload_serial_flash_path_tag 8
#define SyncResponse_FileDownload_sd_card_filename_tag 3
#define SyncResponse_FileDownload_sd_card_path_tag 9
#define SyncResponse_FileDownload_sha1_tag       10
#define SyncResponse_FlashAction_red_tag         1
#define SyncResponse_FlashAction_green_tag       2
#define SyncResponse_FlashAction_blue_tag        3
#define SyncResponse_FlashAction_delay_milliseconds_tag 4
#define SyncResponse_FlashAction_fade_in_tag     5
#define SyncResponse_FlashAction_fade_out_tag    6
#define SyncResponse_FlashAction_rotate_tag      7
#define SyncResponse_FlashAction_alpha_tag       8
#define SyncResponse_LEDAction_type_tag          1
#define SyncResponse_LEDAction_url_tag           2
#define SyncResponse_LEDAction_color_tag         3
#define SyncResponse_LEDAction_alt_color_tag     4
#define SyncResponse_LEDAction_duration_seconds_tag 6
#define SyncResponse_PairingAction_ssid_tag      1
#define SyncResponse_PairingAction_type_tag      2
#define SyncResponse_PillSettings_pill_id_tag    1
#define SyncResponse_PillSettings_pill_color_tag 2
#define SyncResponse_WhiteNoise_start_time_tag   1
#define SyncResponse_WhiteNoise_end_time_tag     2
#define SyncResponse_WhiteNoise_sound_id_tag     3
#define BatchedPillSettings_pill_settings_tag    1
#define SyncResponse_alarm_tag                   6
#define SyncResponse_white_noise_tag             8
#define SyncResponse_reset_device_tag            10
#define SyncResponse_room_conditions_tag         12
#define SyncResponse_files_tag                   13
#define SyncResponse_reset_to_factory_fw_tag     14
#define SyncResponse_audio_control_tag           15
#define SyncResponse_mac_tag                     18
#define SyncResponse_batch_size_tag              19
#define SyncResponse_pill_settings_tag           20
#define SyncResponse_reset_mcu_tag               21
#define SyncResponse_upload_log_level_tag        22
#define SyncResponse_pill_batch_size_tag         23
#define SyncResponse_ring_time_ack_tag           24
#define SyncResponse_room_conditions_lights_off_tag 25

/* Struct field encoding specification for nanopb */
extern const pb_field_t SyncResponse_fields[16];
extern const pb_field_t SyncResponse_FileDownload_fields[11];
extern const pb_field_t SyncResponse_Alarm_fields[7];
extern const pb_field_t SyncResponse_PairingAction_fields[3];
extern const pb_field_t SyncResponse_WhiteNoise_fields[4];
extern const pb_field_t SyncResponse_FlashAction_fields[9];
extern const pb_field_t SyncResponse_LEDAction_fields[6];
extern const pb_field_t SyncResponse_PillSettings_fields[3];
extern const pb_field_t BatchedPillSettings_fields[2];

/* Maximum encoded size of messages (where known) */
#define SyncResponse_Alarm_size                  71
#define SyncResponse_WhiteNoise_size             33
#define SyncResponse_FlashAction_size            61
#define SyncResponse_PillSettings_size           25
#define BatchedPillSettings_size                 54

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
