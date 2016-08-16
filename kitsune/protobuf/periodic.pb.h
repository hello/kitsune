/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.1 at Mon Aug 15 16:05:39 2016. */

#ifndef PB_PERIODIC_PB_H_INCLUDED
#define PB_PERIODIC_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _batched_periodic_data_wifi_access_point_AntennaType {
    batched_periodic_data_wifi_access_point_AntennaType_IFA = 1,
    batched_periodic_data_wifi_access_point_AntennaType_PCB = 2
} batched_periodic_data_wifi_access_point_AntennaType;

/* Struct definitions */
typedef struct _batched_periodic_data {
    pb_callback_t data;
    pb_callback_t device_id;
    int32_t firmware_version;
    bool has_uptime_in_second;
    int32_t uptime_in_second;
    bool has_connected_ssid;
    char connected_ssid[33];
    bool has_ring_time_ack;
    char ring_time_ack[30];
    pb_callback_t scan;
    bool has_messages_in_queue;
    int32_t messages_in_queue;
} batched_periodic_data;

typedef struct _batched_periodic_data_wifi_access_point {
    bool has_ssid;
    char ssid[33];
    bool has_rssi;
    int32_t rssi;
    bool has_antenna;
    batched_periodic_data_wifi_access_point_AntennaType antenna;
} batched_periodic_data_wifi_access_point;

typedef struct _periodic_data_light_data {
    int32_t r;
    int32_t g;
    int32_t b;
    bool has_infrared;
    int32_t infrared;
    bool has_clear;
    int32_t clear;
    bool has_lux_count;
    int32_t lux_count;
    bool has_uv_count;
    int32_t uv_count;
} periodic_data_light_data;

typedef struct _periodic_data {
    bool has_unix_time;
    int32_t unix_time;
    bool has_light;
    int32_t light;
    bool has_temperature;
    int32_t temperature;
    bool has_humidity;
    int32_t humidity;
    bool has_dust;
    int32_t dust;
    bool has_light_variability;
    int32_t light_variability;
    bool has_light_tonality;
    int32_t light_tonality;
    bool has_dust_variability;
    int32_t dust_variability;
    bool has_dust_max;
    int32_t dust_max;
    bool has_dust_min;
    int32_t dust_min;
    bool has_wave_count;
    int32_t wave_count;
    bool has_hold_count;
    int32_t hold_count;
    bool has_audio_num_disturbances;
    int32_t audio_num_disturbances;
    bool has_audio_peak_disturbance_energy_db;
    int32_t audio_peak_disturbance_energy_db;
    bool has_audio_peak_background_energy_db;
    int32_t audio_peak_background_energy_db;
    bool has_audio_peak_energy_db;
    int32_t audio_peak_energy_db;
    bool has_pressure;
    uint32_t pressure;
    bool has_tvoc;
    int32_t tvoc;
    bool has_co2;
    int32_t co2;
    bool has_light_sensor;
    periodic_data_light_data light_sensor;
} periodic_data;

/* Default values for struct fields */

/* Initializer values for message structs */
#define periodic_data_init_default               {false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, periodic_data_light_data_init_default}
#define periodic_data_light_data_init_default    {0, 0, 0, false, 0, false, 0, false, 0, false, 0}
#define batched_periodic_data_init_default       {{{NULL}, NULL}, {{NULL}, NULL}, 0, false, 0, false, "", false, "", {{NULL}, NULL}, false, 0}
#define batched_periodic_data_wifi_access_point_init_default {false, "", false, 0, false, (batched_periodic_data_wifi_access_point_AntennaType)0}
#define periodic_data_init_zero                  {false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, 0, false, periodic_data_light_data_init_zero}
#define periodic_data_light_data_init_zero       {0, 0, 0, false, 0, false, 0, false, 0, false, 0}
#define batched_periodic_data_init_zero          {{{NULL}, NULL}, {{NULL}, NULL}, 0, false, 0, false, "", false, "", {{NULL}, NULL}, false, 0}
#define batched_periodic_data_wifi_access_point_init_zero {false, "", false, 0, false, (batched_periodic_data_wifi_access_point_AntennaType)0}

/* Field tags (for use in manual encoding/decoding) */
#define batched_periodic_data_data_tag           1
#define batched_periodic_data_device_id_tag      2
#define batched_periodic_data_firmware_version_tag 3
#define batched_periodic_data_uptime_in_second_tag 4
#define batched_periodic_data_connected_ssid_tag 7
#define batched_periodic_data_ring_time_ack_tag  9
#define batched_periodic_data_scan_tag           10
#define batched_periodic_data_messages_in_queue_tag 11
#define batched_periodic_data_wifi_access_point_ssid_tag 1
#define batched_periodic_data_wifi_access_point_rssi_tag 2
#define batched_periodic_data_wifi_access_point_antenna_tag 3
#define periodic_data_light_data_r_tag           1
#define periodic_data_light_data_g_tag           2
#define periodic_data_light_data_b_tag           3
#define periodic_data_light_data_infrared_tag    4
#define periodic_data_light_data_clear_tag       5
#define periodic_data_light_data_lux_count_tag   6
#define periodic_data_light_data_uv_count_tag    7
#define periodic_data_unix_time_tag              1
#define periodic_data_light_tag                  2
#define periodic_data_temperature_tag            3
#define periodic_data_humidity_tag               4
#define periodic_data_dust_tag                   5
#define periodic_data_light_variability_tag      8
#define periodic_data_light_tonality_tag         9
#define periodic_data_dust_variability_tag       13
#define periodic_data_dust_max_tag               14
#define periodic_data_dust_min_tag               15
#define periodic_data_wave_count_tag             16
#define periodic_data_hold_count_tag             17
#define periodic_data_audio_num_disturbances_tag 18
#define periodic_data_audio_peak_disturbance_energy_db_tag 19
#define periodic_data_audio_peak_background_energy_db_tag 20
#define periodic_data_audio_peak_energy_db_tag   21
#define periodic_data_pressure_tag               22
#define periodic_data_tvoc_tag                   23
#define periodic_data_co2_tag                    24
#define periodic_data_light_sensor_tag           25

/* Struct field encoding specification for nanopb */
extern const pb_field_t periodic_data_fields[21];
extern const pb_field_t periodic_data_light_data_fields[8];
extern const pb_field_t batched_periodic_data_fields[9];
extern const pb_field_t batched_periodic_data_wifi_access_point_fields[4];

/* Maximum encoded size of messages (where known) */
#define periodic_data_size                       293
#define periodic_data_light_data_size            77
#define batched_periodic_data_wifi_access_point_size 52

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
