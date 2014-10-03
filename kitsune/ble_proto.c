#include "ble_proto.h"
#include "ble_cmd.h"
#include "wlan.h"

#include "uartstdio.h"

#include "FreeRTOS.h"
#define malloc pvPortMalloc
#define free vPortFree

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

    int scanned_wifi_count = _get_wifi_scan_result(wifi_endpoints, MAX_WIFI_EP_PER_SCAN, 10000);  // Shall we have a bg thread scan periodically?
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
                // There is no sl_sl_WlanProfileSet?
                // So I delete all endpoint first.
                _i16 del_ret = sl_WlanProfileDel(0xFF);
                if(del_ret)
                {
                    UARTprintf("Delete all stored endpoint failed, error %d.\n", del_ret);
                }

                // Then add the current one back.
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
        char device_id[device_id_len] = {0};

        uint8_t index = 0;
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

        // Since the device_id is allocated in the stack, we don't need to
        // free protobuf

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

void on_ble_protobuf_command(MorpheusCommand* command)
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
                UARTprintf("Connection attemp failed.\n");
                ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }else{
                // If the wifi connection is set, reply the same message
                // to the phone.
                UARTprintf("Connection attemp issued.\n");
                ble_send_protobuf(command);
            }
            
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
            // Light up LEDs?
            Cmd_led(0,0);
            UARTprintf( "PAIRING MODE\n" );
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        {
            // Get the current wifi connection information.
            _ble_reply_wifi_info();
            UARTprintf( "GET_WIFI\n" );
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA:
        {
            // Pill data received from ANT
        	UARTprintf( "PILL DATA\n" );
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT:
        {   
            // Pill heartbeat received from ANT
        	UARTprintf( "PILL HEARBEAT\n" );
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        {
            // Get morpheus device id request from Nordic
            UARTprintf("GET DEVICE ID\n");
            _reply_device_id();
        }
        break;
    }
}
