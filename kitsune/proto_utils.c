#include "proto_utils.h"
#include "ble_proto.h"
#include "uartstdio.h"
#include "wifi_cmd.h"
#include "kitsune_version.h"

bool _encode_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char* str = *arg;
    if(!str)
    {
    	LOGI("_encode_string_fields: No string to encode\n");
        return false;
    }

    //write tag
    //if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) { // Not sure should do this,
                                                              // This is for encoding byte array
    if (!pb_encode_tag_for_field(stream, field)){
        return 0;
    }

    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

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
static bool _encode_pill_batch_data (pb_ostream_t *stream, const pb_field_t *field, void * const *arg, uint32_t tag){
	int i;
	pilldata_to_encode * data = *(pilldata_to_encode**)arg;

	for( i = 0; i < data->num_pills; ++i ) {
	if(!pb_encode_tag(stream, PB_WT_STRING, tag))
	{
		LOGI("encode_all_pills: Fail to encode tag for pill %s, error %s\n", data->pills[i].device_id, PB_GET_ERROR(stream));
		return false;
	}

	if (!pb_encode_delimited(stream, pill_data_fields, &data->pills[i])){
		LOGI("encode_all_pills: Fail to encode pill, error: %s\n", PB_GET_ERROR(stream));
		return false;
	}
	//LOGI("******************* encode_pill_encode_all_pills: encode pill %s\n", pill_data.deviceId);
	}
	return true;
}
bool encode_all_prox (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return _encode_pill_batch_data(stream, field, arg, batched_pill_data_prox_tag);
}
bool encode_all_pills (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return _encode_pill_batch_data(stream, field, arg, batched_pill_data_pills_tag);
}
#include "hlo_async.h"

bool encode_single_ssid (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	if( arg == NULL ) {
		return true;
	}
    batched_periodic_data_wifi_access_point* ap = (batched_periodic_data_wifi_access_point *)*arg;

	{
		static SlGetRxStatResponse_t rxStat;
		sl_WlanRxStatGet(&rxStat,0);
		if( rxStat.AvarageDataCtrlRssi == 0 ) {
			sl_WlanRxStatStart();  // set statistics mode
		}
		LOGI("RSSI %d %d\n", rxStat.AvarageDataCtrlRssi, rxStat.AvarageMgMntRssi );

		ap->antenna = (batched_periodic_data_wifi_access_point_AntennaType)get_default_antenna();
		ap->has_antenna = true;
		ap->rssi = rxStat.AvarageDataCtrlRssi;
		ap->has_rssi = true;
		wifi_get_connected_ssid( (uint8_t*)ap->ssid, sizeof(ap->ssid) );
		ap->has_ssid = true;
	}

	if(!pb_encode_tag(stream, PB_WT_STRING, batched_periodic_data_scan_tag))
	{
		LOGI("encode_scanned_ssid: Fail to encode tag for ssid, error %s\n", PB_GET_ERROR(stream));
		return false;
	}
	if (!pb_encode_delimited(stream, batched_periodic_data_wifi_access_point_fields, ap )){
		LOGI("encode_scanned_ssid: Fail to encode ssid error: %s\n", PB_GET_ERROR(stream));
		return false;
	}

    return true;
}

bool encode_scanned_ssid (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    int i,n;

	SlWlanNetworkEntry_t scan[10];

	if( arg == NULL ) {
		return true;
	}
	hlo_future_t * scan_future = (hlo_future_t *)*arg;

	n = hlo_future_read(scan_future,scan,sizeof(scan), portMAX_DELAY);
    batched_periodic_data_wifi_access_point ap;

    if( n == 0 ) {
    	return true;
    }

    for( i = 0; i < n; ++i ) {
        if(!pb_encode_tag(stream, PB_WT_STRING, batched_periodic_data_scan_tag))
        {
            LOGI("encode_scanned_ssid: Fail to encode tag for ssid, error %s\n", PB_GET_ERROR(stream));
            return false;
        }
        ap.antenna = (batched_periodic_data_wifi_access_point_AntennaType)scan[i].reserved[0];
        ap.has_antenna = true;
        ap.rssi = scan[i].rssi;
        ap.has_rssi = true;
        scan[i].ssid_len = 0;
        memcpy( ap.ssid, scan[i].ssid, sizeof(ap.ssid));
        ap.has_ssid = true;

        if (!pb_encode_delimited(stream, batched_periodic_data_wifi_access_point_fields, &ap )){
            LOGI("encode_scanned_ssid: Fail to encode ssid error: %s\n", PB_GET_ERROR(stream));
            return false;
        }
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

void pack_batched_periodic_data(batched_periodic_data* batched, periodic_data_to_encode* encode_wrapper)
{
    if(NULL == batched || NULL == encode_wrapper)
    {
        LOGE("null param\n");
        return;
    }

    batched->data.funcs.encode = encode_all_periodic_data;  // This is smart :D
    batched->data.arg = encode_wrapper;
    batched->firmware_version = KIT_VER;
    batched->device_id.funcs.encode = encode_device_id_string;
}

