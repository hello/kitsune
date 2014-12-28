#include "proto_utils.h"
#include "uartstdio.h"
#include "wifi_cmd.h"


bool encode_all_periodic_data (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    int i;
    periodic_data_to_encode * data = *(periodic_data_to_encode**)arg;

    for( i = 0; i < data->num_data; ++i ) {
        if(!pb_encode_tag(stream, PB_WT_STRING, batched_periodic_data_data_tag))
        {
            LOGI("encode_all_periodic_data: Fail to encode tag error %s\n", PB_GET_ERROR(stream));
            return false;
        }

        if (!pb_encode_delimited(stream, periodic_data_fields, &data->data[i])){
            LOGI("encode_all_periodic_data2: Fail to encode error: %s\n", PB_GET_ERROR(stream));
            return false;
        }
        //LOGI("******************* encode_pill_encode_all_pills: encode pill %s\n", pill_data.deviceId);
    }
    return true;
}

bool encode_all_pills (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    int i;
    pilldata_to_encode * data = *(pilldata_to_encode**)arg;

    for( i = 0; i < data->num_pills; ++i ) {
        if(!pb_encode_tag(stream, PB_WT_STRING, batched_pill_data_pills_tag))
        {
            LOGI("encode_all_pills: Fail to encode tag for pill %s, error %s\n", data->pills[i].device_id, PB_GET_ERROR(stream));
            return false;
        }

        if (!pb_encode_delimited(stream, pill_data_fields, &data->pills[i])){
            LOGI("encode_all_pills: Fail to encode pill %s, error: %s\n", data->pills[i].device_id, PB_GET_ERROR(stream));
            return false;
        }
        //LOGI("******************* encode_pill_encode_all_pills: encode pill %s\n", pill_data.deviceId);
    }
    return true;
}


bool encode_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    //char are twice the size, extra 1 for null terminator
    char hex_device_id[2*DEVICE_ID_SZ+1] = {0};
    if(!get_device_id(hex_device_id, sizeof(hex_device_id)))
    {
        return false;
    }

    return pb_encode_tag_for_field(stream, field) && pb_encode_string(stream, (uint8_t*)hex_device_id, strlen(hex_device_id));
}

