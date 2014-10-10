
#include "ble_proto.h"
#include "ble_cmd.h"
#include "wlan.h"

#include "wifi_cmd.h"
#include "uartstdio.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef malloc
#define malloc pvPortMalloc
#define free vPortFree
#endif

static int _get_wifi_scan_result(Sl_WlanNetworkEntry_t* entries, uint16_t entry_len, uint32_t scan_duration_ms)
{
    if(scan_duration_ms < 1000)
    {
        return 0;
    }

    unsigned long IntervalVal = 60;

    unsigned char policyOpt = SL_CONNECTION_POLICY(0, 0, 0, 0, 0);
    int lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION , policyOpt, NULL, 0);


    // enable scan
    policyOpt = SL_SCAN_POLICY(1);

    // set scan policy - this starts the scan
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt, (unsigned char *)(IntervalVal), sizeof(IntervalVal));


    // delay specific milli seconds to verify scan is started
    vTaskDelay(scan_duration_ms);

    // lRetVal indicates the valid number of entries
    // The scan results are occupied in netEntries[]
    lRetVal = sl_WlanGetNetworkList(0, entry_len, entries);

    // Disable scan
    policyOpt = SL_SCAN_POLICY(0);

    // set scan policy - this stops the scan
    sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt,
                            (unsigned char *)(IntervalVal), sizeof(IntervalVal));

    return lRetVal;

}


bool set_wifi(const char* ssid, const char* password)
{
    Sl_WlanNetworkEntry_t wifi_endpoints[MAX_WIFI_EP_PER_SCAN];
    memset(wifi_endpoints, 0, sizeof(wifi_endpoints));

    int scanned_wifi_count = _get_wifi_scan_result(wifi_endpoints, MAX_WIFI_EP_PER_SCAN, 1000);  // Shall we have a bg thread scan periodically?
    if(scanned_wifi_count == 0)
    {
        return 0;
    }

    int i = 0;
    for(i = 0; i < scanned_wifi_count; i++)
    {
        Sl_WlanNetworkEntry_t wifi_endpoint = wifi_endpoints[i];
        if(strcmp((const char*)wifi_endpoint.ssid, ssid) == 0)
        {
            SlSecParams_t secParams;
            memset(&secParams, 0, sizeof(SlSecParams_t));

            if( wifi_endpoint.sec_type == 3 ) {
            	wifi_endpoint.sec_type = 2;
            }
            secParams.Key = (signed char*)password;
            secParams.KeyLen = password == NULL ? 0 : strlen(password);
            secParams.Type = wifi_endpoint.sec_type;

            SlSecParams_t* secParamsPtr = wifi_endpoint.sec_type == SL_SEC_TYPE_OPEN ? NULL : &secParams;

            // We don't support all the security types in this implementation.
            int16_t ret = sl_WlanConnect((_i8*)ssid, strlen(ssid), NULL, secParamsPtr, 0);
            if(ret == 0 || ret == -71)
            {
                // To make things simple in the first pass implementation, 
                // we only store one endpoint.
/*
                // There is no sl_sl_WlanProfileSet?
                // So I delete all endpoint first.
                _i16 del_ret = sl_WlanProfileDel(0xFF);
                if(del_ret)
                {
                    UARTprintf("Delete all stored endpoint failed, error %d.\n", del_ret);
                }

                // Then add the current one back.
*/
                _i16 profile_add_ret = sl_WlanProfileAdd((_i8*)ssid, strlen(ssid), NULL, secParamsPtr, NULL, 0, 0);
                if(profile_add_ret < 0)
                {
                    UARTprintf("Save connected endpoint failed, error %d.\n", profile_add_ret);
                }

                return 1;
            }

        }
    }

    

    return 0;
}

static void _reply_device_id()
{
    uint8_t mac_len = SL_MAC_ADDR_LEN;
    uint8_t mac[SL_MAC_ADDR_LEN] = {0};

    int32_t ret = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
    if(ret == 0)
    {
        uint8_t device_id_len = SL_MAC_ADDR_LEN * 2 + 1;  // hex string representation
        char* device_id = malloc(device_id_len);
        memset(device_id, 0, device_id_len);

        uint8_t i = 0;
        for(i = 0; i < SL_MAC_ADDR_LEN; i++){
            sprintf(device_id[i * 2], "%02X", mac[i]);  // It has sprintf!
        }

        UARTprintf("Morpheus device id: %s\n", device_id);
        MorpheusCommand reply_command;
        memset(&reply_command, 0, sizeof(reply_command));
        reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;
        reply_command.version = PROTOBUF_VERSION;

        reply_command.deviceId.arg = device_id;
        ble_send_protobuf(&reply_command);

        free_protobuf_command(&reply_command);

    }else{
        UARTprintf("Get Mac address failed, error %d.\n", ret);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
    }
}

/*
* Get the current saved WIFI profile and send it back via BLE
*
*/
static void _ble_reply_wifi_info(){
    int8_t*  name = malloc(32);  // due to wlan.h
    if(!name)
    {
        UARTprintf("Not enough memory.\n");
        ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        return;
    }

    memset(name, 0, 32);
    int16_t name_len = 0;
    uint8_t mac_addr[6] = {0};
    SlSecParams_t sec_params = {0};
    SlGetSecParamsExt_t secExt_params = {0};
    unsigned long priority  = 0;

    int16_t get_ret = sl_WlanProfileGet(0, name, &name_len, mac_addr, &sec_params, &secExt_params, &priority);
    if(get_ret == -1)
    {
        UARTprintf("Get wifi endpoint failed, error %d.\n", get_ret);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);

    }else{
        MorpheusCommand reply_command;
        memset(&reply_command, 0, sizeof(reply_command));
        reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT;
        reply_command.version = PROTOBUF_VERSION;

        size_t len = strlen((char*)name) + 1;
        char* ssid = malloc(len);

        if(ssid)
        {
            memset(ssid, 0, len);
            memcpy(ssid, name, strlen((const char*)name));

            reply_command.wifiSSID.arg = ssid;
            ble_send_protobuf(&reply_command);
            free(ssid);
        }else{
            UARTprintf("Not enough memory.\n");
            ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        }

    }

    free(name);
}

int Cmd_led(int argc, char *argv[]);
#include "wifi_cmd.h"
periodic_data_pill_data_container pill_list[MAX_PILLS] = {0};

int scan_pill_list(periodic_data_pill_data_container* p, char * device_id) {
	int i;
	for (i = 0; i < MAX_PILLS && p[i].magic == PILL_MAGIC; ++i) {
		if (strcmp(p[i].id, device_id) == 0) {
			break;
		}
	}
	if (i == MAX_PILLS) {
		UARTprintf(" too many pills, overwriting\n ");
		i=0;
	}
	return i;
}

static void _process_encrypted_pill_data(const MorpheusCommand* command)
{
    if (command->motionDataEntrypted.arg) {
        int i;
        if (xSemaphoreTake(pill_smphr, portMAX_DELAY)) {
            i = scan_pill_list(pill_list, command->deviceId.arg);

            memset(pill_list[i].id, 0, PILL_ID_LEN + 1);  // Just in case
            memcpy(pill_list[i].id, command->deviceId.arg, PILL_ID_LEN);

            if(pill_list[i].pill_data.motionDataEncrypted.arg)
            {
                // If we see there is old data, first release them,
                // or we will get memory leak.
                array_data* old_data = (array_data*)pill_list[i].pill_data.motionDataEncrypted.arg;
                if(old_data->buffer)
                {
                    free(old_data->buffer);
                }
                free(old_data);
                pill_list[i].pill_data.motionDataEncrypted.arg = NULL;
                pill_list[i].pill_data.motionDataEncrypted.funcs.encode = NULL;
            }
            

            const array_data* array = (array_data*)command->motionDataEntrypted.arg;  // This thing will be free when this function exits
            array_data* array_cp = malloc(sizeof(array_data));
            if(!array_cp){
                UARTprintf("No memory\n");

            }else{
                char* encrypted_data = malloc(array->length);
                if(!encrypted_data){
                    free(array_cp);
                    UARTprintf("No memory\n");
                }else{
                    array_cp->buffer = encrypted_data;
                    array_cp->length = array->length;
                    memcpy(encrypted_data, array->buffer, array->length);

                    pill_list[i].pill_data.motionDataEncrypted.arg = array_cp;
                    pill_list[i].magic = PILL_MAGIC;
                }
            }

            UARTprintf("PILL DATA FROM ID: %s, length: %d\n", command->deviceId.arg, array->length);
            int i = 0;
            for(i = 0; i < array->length; i++){
                UARTprintf( "%x", array->buffer[i] );

            }
            UARTprintf("\n");

            xSemaphoreGive(pill_smphr);
        }
    }
}

static void _process_pill_heartbeat(const MorpheusCommand* command)
{
    int i = 0;
    if (xSemaphoreTake(pill_smphr, portMAX_DELAY)) {
        i = scan_pill_list(pill_list, command->deviceId.arg);

        memset(pill_list[i].id, 0, PILL_ID_LEN + 1);  // Just in case
        memcpy(pill_list[i].id, command->deviceId.arg, PILL_ID_LEN);
        pill_list[i].magic = PILL_MAGIC;

        // Pill heartbeat received from ANT
        UARTprintf("PILL HEARBEAT %s\n", command->deviceId.arg);

        if (command->has_batteryLevel) {
            pill_list[i].pill_data.batteryLevel = command->batteryLevel;
            UARTprintf("PILL BATTERY %d\n", command->batteryLevel);
        }
        if (command->has_batteryLevel) {
            pill_list[i].pill_data.uptime = command->uptime;
            UARTprintf("PILL UPTIME %d\n", command->uptime);
        }

        if(command->has_firmwareVersion) {
            pill_list[i].pill_data.firmwareVersion = command->firmwareVersion;
            UARTprintf("PILL FirmwareVersion %d\n", command->firmwareVersion);
        }
        xSemaphoreGive(pill_smphr);
    }
}

static void _send_response_to_ble(const char* buffer, size_t len)
{
    const char* header_content_len = "Content-Length: "
    char * content = strstr(buffer, "\r\n\r\n") + 4;
    char * len_str = strstr(buffer, header_content_len) + strlen(header_content_len);
    if (len_str == NULL) {
        UARTprintf("Invalid response, Content-Length header not found.\n");
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }


    int resp_ok = match("2..", buffer);
    if (!resp_ok) {
        UARTprintf("Invalid response, %s endpoint return failure.\n", PILL_REGISTER_ENDPOINT);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }


    int content_len = atoi(len_str);
    MorpheusCommand response;
    memset(&response, 0, sizeof(response));
    ble_proto_assign_decode_funcs(&response);

    if(decode_rx_data_pb((unsigned char*)response_buffer, content_len, MorpheusCommand_fields, &response, sizeof(response)) != 0)
    {
        UARTprintf("Invalid response, protobuf decryption & decode failed.\n");
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
    }else{

        ble_send_protobuf(&response);
    }
    ble_proto_free_command(&response);
}

void on_ble_protobuf_command(const MorpheusCommand* command)
{
    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        {
            const char* ssid = command->wifiSSID.arg;
            char* password = command->wifiPassword.arg;

            // I can get the Mac address as well, but not sure it is necessary.

            // Just call API to connect to WIFI.
            UARTprintf("Wifi SSID %s, pswd %s \n", ssid, password);
            if(!set_wifi(ssid, (char*)password))
            {
                UARTprintf("Connection attempt failed.\n");
                ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }else{
                // If the wifi connection is set, reply
                MorpheusCommand reply_command;
                memset(&reply_command, 0, sizeof(reply_command));
                reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT;
                reply_command.version = PROTOBUF_VERSION;
                reply_command.wifiSSID.arg = (void*)ssid;

                UARTprintf("Connection attempt issued.\n");
                ble_send_protobuf(&reply_command);
            }
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
            // Light up LEDs?
            Cmd_led(0,0);
            UARTprintf( "PAIRING MODE %s\n", command->deviceId.arg );
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        {
            // Get the current wifi connection information.
            _ble_reply_wifi_info();
            UARTprintf( "GET_WIFI %s\n", command->deviceId.arg );
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        {
            // Get morpheus device id request from Nordic
            UARTprintf("GET DEVICE ID\n");
            _reply_device_id();
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA: 
        {
    		// Pill data received from ANT
            UARTprintf("PILL DATA\n");
    		_process_encrypted_pill_data(command);
    	}
		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT: 
        {
            UARTprintf("PILL HEARTBEAT\n");
    		_process_pill_heartbeat(command);
    	}
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL:
        {
            UARTprintf("PAIR PILL\n");
            char response_buffer[256] = {0};
            ble_proto_assign_encode_funcs(command);
            int ret = send_data_pb(DATA_SERVER, PILL_REGISTER_ENDPOINT, response_buffer, sizeof(response_buffer), MorpheusCommand_fields, command);

            if(ret == 0)
            {
                _send_response_to_ble(response_buffer, sizeof(response_buffer));
            }
            
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
        {
            UARTprintf("PAIR SENSE\n");
            char response_buffer[256] = {0};
            ble_proto_assign_encode_funcs(command);
            int ret = send_data_pb(DATA_SERVER, MORPHEUS_REGISTER_ENDPOINT, response_buffer, sizeof(response_buffer), MorpheusCommand_fields, command);

            if(ret == 0)
            {
                _send_response_to_ble(response_buffer, sizeof(response_buffer));
            }
        }
        break;
	}
}
