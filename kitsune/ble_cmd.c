
#include <string.h>

#include "ble_cmd.h"
#include "ble_proto.h"

#include "uartstdio.h"

#include "wifi_cmd.h"
#include "spi_cmd.h"
#include "pill_settings.h"
#include "proto_utils.h"

static bool _encode_bytes_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);


static bool _encode_wifi_scan_result_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    SlWlanNetworkEntry_t* wifi_ap = (SlWlanNetworkEntry_t*)*arg;
    if(!wifi_ap)
    {
        LOGI("_encode_string_fields: No string to encode\n");
        return false;
    }

	wifi_endpoint data = {0};
	char ssid[SL_WLAN_SSID_MAX_LENGTH + 1] = {0};
	strncpy(ssid, (char*)wifi_ap->Ssid, SL_WLAN_SSID_MAX_LENGTH + 1);
	data.ssid.funcs.encode = _encode_string_fields;
	data.ssid.arg = ssid;

	array_data arr = {0};
	arr.length = SL_WLAN_BSSID_LENGTH;
	arr.buffer = wifi_ap->Bssid;
	data.bssid.funcs.encode = _encode_bytes_fields;
	data.bssid.arg = &arr;


	switch(wifi_ap->SecurityInfo)
	{
		case SL_WLAN_SEC_TYPE_OPEN:
		data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_OPEN;
		break;

		case SL_WLAN_SEC_TYPE_WEP:
		data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WEP;
		break;

		case SL_WLAN_SEC_TYPE_WPA:
		data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA;
		break;

		case SL_WLAN_SEC_TYPE_WPA_WPA2:
		data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA2;
		break;
	}

	data.rssi = wifi_ap->Rssi;

	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)){
		LOGI("Fail to encode tag for wifi ap %s\r\n", wifi_ap->Ssid);
	}else{

		pb_ostream_t sizestream = { 0 };
		if(!pb_encode(&sizestream, wifi_endpoint_fields, &data)){
			LOGI("Failed to retreive length for ssid %s\n", data.ssid.arg);
			return false;
		}

		if (!pb_encode_varint(stream, sizestream.bytes_written)){
			LOGI("Failed to write length\n");
			return false;
		}

		if (!pb_encode(stream, wifi_endpoint_fields, &data)){
			LOGI("Fail to encode wifi_endpoint_fields %s\r\n", data.ssid.arg);
		}else{
			LOGI("WIFI %s encoded\n", data.ssid.arg);
		}
	}


    return true;
}

static bool _encode_bytes_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    array_data* array = *arg;
    if(!array)
    {
    	LOGI("_encode_bytes_fields: No bytes to encode\n");
        return false;
    }

    //write tag
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return 0;
    }

    return pb_encode_string(stream, (uint8_t*)array->buffer, array->length);
}


bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > MAX_STRING_LEN - 1)
    {
    	LOGI("_decode_string_field: String too long to decode\n");
        return false;
    }
    
    uint8_t* str = pvPortMalloc(stream->bytes_left + 1);
    if(!str)
    {
    	LOGI("_decode_string_field: Not enough memory\n");
        return false;
    }

    memset(str, 0, stream->bytes_left + 1);
    if (!pb_read(stream, str, stream->bytes_left))
    {
    	LOGI("_decode_string_field: Cannot read string\n");
        vPortFree(str);  // Remember to vPortFree if read failed.
        return false;
    }
    str = (uint8_t*)pvPortRealloc( str, strlen((const char*)str)+1);

    *arg = str;

    return true;
}
#include "hlo_proto_tools.h"
void on_morpheus_protobuf_arrival(uint8_t* protobuf, size_t len)
{
    if(!protobuf)
    {
        LOGI("Invalid parameter.\r\n");
        return;
    }
    hlo_future_t * future_proto = MorpheusCommand_from_buffer(protobuf, len);
    if(future_proto){
    	int ret = hlo_future_read(future_proto,NULL, 0, portMAX_DELAY);
    	if(ret >= 0 && future_proto->buf){
    		on_ble_protobuf_command(future_proto->buf);
    	}
    	hlo_future_destroy(future_proto);
    }
}

void ble_proto_assign_decode_funcs(MorpheusCommand* command)
{
	command->accountId.funcs.decode = _decode_string_field;
	command->deviceId.funcs.decode = _decode_string_field;
	command->wifiName.funcs.decode = _decode_string_field;
	command->wifiSSID.funcs.decode = _decode_string_field;
	command->wifiPassword.funcs.decode = _decode_string_field;

	command->accountId.arg = NULL;
	command->deviceId.arg = NULL;
	command->wifiName.arg = NULL;
	command->wifiSSID.arg = NULL;
	command->wifiPassword.arg = NULL;
}

void ble_proto_assign_encode_funcs( MorpheusCommand* command)
{
	command->accountId.funcs.decode = NULL;
	command->deviceId.funcs.decode = NULL;
	command->wifiName.funcs.decode = NULL;
	command->wifiSSID.funcs.decode = NULL;
	command->wifiPassword.funcs.decode = NULL;

    if(command->accountId.arg != NULL )
    {
        command->accountId.funcs.encode = _encode_string_fields;
    }

    if(command->deviceId.arg != NULL )
    {
        command->deviceId.funcs.encode = _encode_string_fields;
    }

    if(command->wifiName.arg != NULL )
    {
        command->wifiName.funcs.encode = _encode_string_fields;
    }

    if(command->wifiSSID.arg != NULL )
    {
        command->wifiSSID.funcs.encode = _encode_string_fields;
    }

    if(command->wifiPassword.arg != NULL )
    {
        command->wifiPassword.funcs.encode = _encode_string_fields;
    }


    if(command->wifi_scan_result.arg != NULL )
    {
        command->wifi_scan_result.funcs.encode = _encode_wifi_scan_result_fields;
    }
}

bool ble_reply_protobuf_error(ErrorType error_type)
{
    MorpheusCommand morpheus_command = {0};
    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERROR;

    morpheus_command.has_error = true;
    morpheus_command.error = error_type;

    return ble_send_protobuf(&morpheus_command);
}


bool ble_send_protobuf(MorpheusCommand* command)
{
	int size;
	hlo_future_t * result =  buffer_from_MorpheusCommand(command);
	size = hlo_future_read(result,NULL,0,portMAX_DELAY);
	vTaskDelay(20);
    if(size >= 0){
    	int i;
        i = spi_write(result->buf_size, result->buf);
        LOGI("spiwrite: %d",i);
        hlo_future_destroy(result);
        return true;
    }
    hlo_future_destroy(result);
    return false;
}

void ble_proto_get_morpheus_command(pb_field_t ** fields, void ** structdata){
	*fields = (pb_field_t *)MorpheusCommand_fields;
	*structdata = pvPortMalloc(sizeof(MorpheusCommand));
	assert(*structdata);
	if( *structdata ) {
		ble_proto_assign_decode_funcs(*structdata);
		memset( *structdata, 0, sizeof(MorpheusCommand) );
	}
}
void ble_proto_free_morpheus_command(void * structdata){
	if( structdata ) {
	    ble_proto_free_command( structdata );
		vPortFree( structdata );
	}
}

void ble_proto_free_command(MorpheusCommand* command)
{
    if(!command)
    {
        LOGI("Inavlid parameter.\r\n");
        return;
    }

    if(command->accountId.arg)
    {
        vPortFree(command->accountId.arg);
        command->accountId.arg = NULL;
    }

    if(command->deviceId.arg)
    {
        vPortFree(command->deviceId.arg);
        command->deviceId.arg = NULL;
    }

    if(command->wifiName.arg)
    {
        vPortFree(command->wifiName.arg);
        command->wifiName.arg = NULL;
    }

    if(command->wifiSSID.arg)
    {
        vPortFree(command->wifiSSID.arg);
        command->wifiSSID.arg = NULL;
    }

    if(command->wifiPassword.arg)
    {
        vPortFree(command->wifiPassword.arg);
        command->wifiPassword.arg = NULL;
    }


    if(command->wifi_scan_result.arg)
    {
        vPortFree(command->wifi_scan_result.arg);
        command->wifi_scan_result.arg = NULL;
    }
}

