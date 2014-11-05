
#include <string.h>

#include "ble_cmd.h"
#include "ble_proto.h"

#include "uartstdio.h"

#include "wifi_cmd.h"
#include "spi_cmd.h"

static bool _encode_bytes_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

static bool _encode_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char* str = *arg;
    if(!str)
    {
    	UARTprintf("_encode_string_fields: No string to encode\n");
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

static bool _encode_wifi_scan_result_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    Sl_WlanNetworkEntry_t* wifi_aps = *arg;
    if(!wifi_aps)
    {
        UARTprintf("_encode_string_fields: No string to encode\n");
        return false;
    }

    int i = 0;
    for(i = 0; i < MAX_WIFI_EP_PER_SCAN; i++)
    {
        Sl_WlanNetworkEntry_t wifi_ap = wifi_aps[i];
        if(wifi_ap.ssid_len == 0)
        {
            break;  // Skip empty SSID, or null item
        }

        

        wifi_endpoint data = {0};
        char ssid[MAXIMAL_SSID_LENGTH + 1] = {0};
        memcpy(ssid, wifi_ap.ssid, wifi_ap.ssid_len);
        data.ssid.funcs.encode = _encode_string_fields;
        data.ssid.arg = ssid;

		array_data arr = {0};
		arr.length = SL_BSSID_LENGTH;
		arr.buffer = wifi_ap.bssid;
		data.bssid.funcs.encode = _encode_bytes_fields;
		data.bssid.arg = &arr;


        switch(wifi_ap.sec_type)
        {
            case SL_SCAN_SEC_TYPE_OPEN:
            data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_OPEN;
            break;

            case SL_SCAN_SEC_TYPE_WEP:
            data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WEP;
            break;

            case SL_SCAN_SEC_TYPE_WPA:
            data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA;
            break;

            case SL_SCAN_SEC_TYPE_WPA2:
            data.security_type = wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA2;
            break;
        }

        data.rssi = wifi_ap.rssi;

        if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)){
            UARTprintf("Fail to encode tag for wifi ap %s\r\n", wifi_ap.ssid);
        }else{

            pb_ostream_t sizestream = { 0 };
            if(!pb_encode(&sizestream, wifi_endpoint_fields, &data)){
                UARTprintf("Failed to retreive length for ssid %s\n", data.ssid.arg);
                continue;
            }

            if (!pb_encode_varint(stream, sizestream.bytes_written)){
                UARTprintf("Failed to write length\n");
                continue;
            }

            if (!pb_encode(stream, wifi_endpoint_fields, &data)){
                UARTprintf("Fail to encode wifi_endpoint_fields %s\r\n", data.ssid.arg);
            }else{
                UARTprintf("WIFI %s encoded\n", data.ssid.arg);
            }
        }
    }

    return true;
}

static bool _encode_bytes_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    array_data* array = *arg;
    if(!array)
    {
    	UARTprintf("_encode_bytes_fields: No bytes to encode\n");
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
    	UARTprintf("_decode_string_field: String too long to decode\n");
        return false;
    }
    
    uint8_t* str = pvPortMalloc(stream->bytes_left + 1);
    if(!str)
    {
    	UARTprintf("_decode_string_field: Not enought memory\n");
        return false;
    }

    memset(str, 0, stream->bytes_left + 1);
    if (!pb_read(stream, str, stream->bytes_left))
    {
    	UARTprintf("_decode_string_field: Cannot read string\n");
        vPortFree(str);  // Remember to vPortFree if read failed.
        return false;
    }
    *arg = str;

    return true;
}

static bool _decode_bytes_field(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > MAX_STRING_LEN - 1)
    {
    	UARTprintf("_decode_bytes_fields: Data tooo long\n");
        return false;
    }

    int length = stream->bytes_left;
    size_t buffer_len = stream->bytes_left + sizeof(array_data);
    uint8_t* buffer = pvPortMalloc(buffer_len);

    if(!buffer)
    {
    	UARTprintf("_decode_bytes_fields: Out of memory\n");
        return false;
    }

    memset(buffer, 0, buffer_len);
    array_data* array = (array_data*)&buffer[length];
    if (!pb_read(stream, buffer, stream->bytes_left))
    {
    	UARTprintf("_decode_bytes_fields: Failed to read data\n");
        vPortFree(buffer);
        return false;
    }

    array->buffer = buffer;
    array->length = length;

    *arg = array;

    return true;
}




void on_morpheus_protobuf_arrival(uint8_t* protobuf, size_t len)
{
    if(!protobuf)
    {
        UARTprintf("Invalid parameter.\r\n");
        return;
    }

    MorpheusCommand command;
    memset(&command, 0, sizeof(command));

    
    ble_proto_assign_decode_funcs(&command);

    pb_istream_t stream = pb_istream_from_buffer(protobuf, len);
    bool status = pb_decode(&stream, MorpheusCommand_fields, &command);
    ble_proto_remove_decode_funcs(&command);

    if (!status)
    {
        UARTprintf("Decoding protobuf failed, error: ");
        UARTprintf(PB_GET_ERROR(&stream));
        UARTprintf("\r\n");
    }else{

        on_ble_protobuf_command(&command);
    }

    ble_proto_free_command(&command);
    
}

void ble_proto_assign_decode_funcs(MorpheusCommand* command)
{
    if(NULL == command->accountId.funcs.decode)
    {
        command->accountId.funcs.decode = _decode_string_field;
        command->accountId.arg = NULL;
    }

    if(NULL == command->deviceId.funcs.decode)
    {
        command->deviceId.funcs.decode = _decode_string_field;
        command->deviceId.arg = NULL;
    }

    if(NULL == command->wifiName.funcs.decode)
    {
        command->wifiName.funcs.decode = _decode_string_field;
        command->wifiName.arg = NULL;
    }

    if(NULL == command->wifiSSID.funcs.decode)
    {
        command->wifiSSID.funcs.decode = _decode_string_field;
        command->wifiSSID.arg = NULL;
    }

    if(NULL == command->wifiPassword.funcs.decode)
    {
        command->wifiPassword.funcs.decode = _decode_string_field;
        command->wifiPassword.arg = NULL;
    }

    if(NULL == command->motionDataEntrypted.funcs.decode)
    {
        command->motionDataEntrypted.funcs.decode = _decode_bytes_field;
        command->motionDataEntrypted.arg = NULL;
    }
}

void ble_proto_remove_decode_funcs(MorpheusCommand* command)
{
    if(command->accountId.funcs.decode)
    {
        command->accountId.funcs.decode = NULL;
    }

    if(command->deviceId.funcs.decode)
    {
        command->deviceId.funcs.decode = NULL;
    }

    if(command->wifiName.funcs.decode)
    {
        command->wifiName.funcs.decode = NULL;
    }

    if(command->wifiSSID.funcs.decode)
    {
        command->wifiSSID.funcs.decode = NULL;
    }

    if(command->wifiPassword.funcs.decode)
    {
        command->wifiPassword.funcs.decode = NULL;
    }

    if(command->motionDataEntrypted.funcs.decode)
    {
        command->motionDataEntrypted.funcs.decode = NULL;
    }
}

void ble_proto_assign_encode_funcs( MorpheusCommand* command)
{
    if(command->accountId.arg != NULL && command->accountId.funcs.encode == NULL)
    {
        command->accountId.funcs.encode = _encode_string_fields;
    }

    if(command->deviceId.arg != NULL && command->deviceId.funcs.encode == NULL)
    {
        command->deviceId.funcs.encode = _encode_string_fields;
    }

    if(command->wifiName.arg != NULL && command->wifiName.funcs.encode == NULL)
    {
        command->wifiName.funcs.encode = _encode_string_fields;
    }

    if(command->wifiSSID.arg != NULL && command->wifiSSID.funcs.encode == NULL)
    {
        command->wifiSSID.funcs.encode = _encode_string_fields;
    }

    if(command->wifiPassword.arg != NULL && command->wifiPassword.funcs.encode == NULL)
    {
        command->wifiPassword.funcs.encode = _encode_string_fields;
    }

    if(command->motionDataEntrypted.arg != NULL && command->motionDataEntrypted.funcs.encode == NULL)
    {
        command->motionDataEntrypted.funcs.encode = _encode_bytes_fields;
    }

    if(command->wifi_scan_result.arg != NULL && command->wifi_scan_result.funcs.encode == NULL)
    {
        command->wifi_scan_result.funcs.encode = _encode_wifi_scan_result_fields;
    }
}

bool ble_reply_protobuf_error(ErrorType error_type)
{
	uint8_t _error_buf[20];
    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(morpheus_command));
    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERROR;
    morpheus_command.version = PROTOBUF_VERSION;

    morpheus_command.has_error = true;
    morpheus_command.error = error_type;

    memset(_error_buf, 0, sizeof(_error_buf));

    pb_ostream_t stream = pb_ostream_from_buffer(_error_buf, sizeof(_error_buf));
    bool status = pb_encode(&stream, MorpheusCommand_fields, &morpheus_command);
    
    if(status)
    {
        size_t protobuf_len = stream.bytes_written;
        spi_write(protobuf_len, _error_buf);
    }else{
        UARTprintf("encode protobuf failed: %s\r\n", PB_GET_ERROR(&stream));
    }

    return status;
}


bool ble_send_protobuf(MorpheusCommand* command)
{
    if(!command){
        UARTprintf("Inavlid parameter.\r\n");
        return false;
    }

    command->version = PROTOBUF_VERSION;
    ble_proto_assign_encode_funcs(command);


    pb_ostream_t stream = {0};
    pb_encode(&stream, MorpheusCommand_fields, command);

    if(!stream.bytes_written)
    {
        return false;
    }

    uint8_t* heap_page = pvPortMalloc(stream.bytes_written);
    if(!heap_page)
    {
        UARTprintf("Not enough memory.\r\n");
        return false;
    }

    memset(heap_page, 0, stream.bytes_written);
    stream = pb_ostream_from_buffer(heap_page, stream.bytes_written);

    bool status = pb_encode(&stream, MorpheusCommand_fields, command);
    
    if(status)
    {
    	int i;

        size_t protobuf_len = stream.bytes_written;
        i = spi_write(protobuf_len, heap_page);
        UARTprintf("spiwrite: %d",i);

    }else{
        UARTprintf("encode protobuf failed: ");
        UARTprintf(PB_GET_ERROR(&stream));
        UARTprintf("\r\n");
    }
    vPortFree(heap_page);

    return status;
}


void ble_proto_free_command(MorpheusCommand* command)
{
    if(!command)
    {
        UARTprintf("Inavlid parameter.\r\n");
        return;
    }

    if(!command->accountId.arg)
    {
        vPortFree(command->accountId.arg);
        command->accountId.arg = NULL;
    }

    if(!command->deviceId.arg)
    {
        vPortFree(command->deviceId.arg);
        command->deviceId.arg = NULL;
    }

    if(!command->wifiName.arg)
    {
        vPortFree(command->wifiName.arg);
        command->wifiName.arg = NULL;
    }

    if(!command->wifiSSID.arg)
    {
        vPortFree(command->wifiSSID.arg);
        command->wifiSSID.arg = NULL;
    }

    if(!command->wifiPassword.arg)
    {
        vPortFree(command->wifiPassword.arg);
        command->wifiPassword.arg = NULL;
    }

    if(!command->motionDataEntrypted.arg)
    {
        array_data* array = (array_data*)command->motionDataEntrypted.arg;
        vPortFree(array->buffer);  // Buffer and holder are in the same block, holder->buffer points to the start addr
        command->motionDataEntrypted.arg = NULL;
    }

    if(!command->wifi_scan_result.arg)
    {
        vPortFree(command->wifi_scan_result.arg);
        command->wifi_scan_result.arg = NULL;
    }
}


int Cmd_factory_reset(int argc, char* argv[])
{
    wifi_reset();

	MorpheusCommand morpheusCommand = {0};
	morpheusCommand.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET;
	ble_send_protobuf(&morpheusCommand);  // Send the protobuf to topboard

	return 0;
}
