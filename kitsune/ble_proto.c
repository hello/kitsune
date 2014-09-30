#include "ble_proto.h"
#include "wlan.h"

static char _error_buf[20];

bool set_wifi(const char* ssid, const char* password)
{
    SlSecParams_t secParams;
    memset(&secParams, 0, sizeof(SlSecParams_t));

    for(int security_type = SL_SEC_TYPE_OPEN; security_type < SL_SEC_TYPE_P2P_PIN_AUTO; security_type++)
    {
        secParams.Key = password;
        secParams.KeyLen = password == NULL ? 0 : strlen(password);
        secParams.Type = security_type;

        const SlSecParams_t* secParamsPtr = security_type == SL_SEC_TYPE_OPEN ? NULL : &secParams;

        // We don't support all the security types in this implementation.
        uint16_t ret = sl_WlanConnect((_i8*)ssid, strlen(ssid), NULL, secParamsPtr, 0);
        if(ret == 0 || ret == -71)
        {
            // To make things simple in the first pass implementation, 
            // we only store one endpoint.
            // There is no sl_sl_WlanProfileSet?
            // So I delete all endpoint first.
            _i16 del_ret = sl_WlanProfileDel(0xFF);
            if(del_ret)
            {
                UARTprintf("Delete all stored endpoint failed, error %d.\r\n", del_ret);
            }

            // Then add the current one back.
            _i16 profile_add_ret = sl_WlanProfileAdd((_i8*)ssid, strlen(ssid), NULL, secParamsPtr, NULL, 0, 0);
            if(profile_add_ret < 0)
            {
                UARTprintf("Save connected endpoint failed, error %d.\r\n", profile_add_ret);
            }

            return 1;
        }
    }

    return 0;
}

/*
* Get the current saved WIFI profile and send it back via BLE
*
*/
static void _ble_reply_wifi_info(){
    _i8*  name = malloc(32);  // due to wlan.h
    if(!name)
    {
        UARTprintf("Not enough memory.\r\n");
        ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        return;
    }

    memset(name, 0, 32);
    _i16 name_len = 0;
    _u8 mac_addr[6] = {0};
    SlSecParams_t sec_params = {0};
    SlGetSecParamsExt_t secExt_params = {0};
    _u32 priority  = 0;

    _i16 get_ret = sl_WlanProfileGet(0, name, &name_len, mac_addr, &sec_params, &secExt_params, &priority);
    if(get_ret == -1)
    {
        UARTprintf("Get wifi endpoint failed, error %d.\r\n", get_ret);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);

    }else{
        MorpheusCommand reply_command;
        memset(&reply_command, 0, sizeof(reply_command));
        reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT;
        reply_command.version = PROTOBUF_VERSION;

        size_t len = strlen(name) + 1;
        char* ssid = malloc(len);

        if(ssid)
        {
            memset(ssid, 0, len);
            memcpy(ssid, name, strlen(name));

            reply_command.wifiSSID.arg = ssid;
            ble_send_protobuf(&reply_command);
            free(ssid);
        }else{
            UARTprintf("Not enough memory.\r\n");
            ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        }

    }

    free(name);
}



void on_ble_protobuf_command(const MorpheusCommand* command)
{
    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        {
            const char* ssid = command->wifiSSID.arg;
            const char* password = command->wifiPassword.arg;
            // I can get the Mac address as well, but not sure it is necessary.

            // Just call API to connect to WIFI.
            UARTprintf("Wifi SSID %s, pswd %s \r\n", ssid, password);
            if(!set_wifi(ssid, password))
            {
                ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }else{
                // If the wifi connection is set, reply the same message
                // to the phone.
                ble_send_protobuf(command);
            }
            
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
            // Light up LEDs?
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        {
            // Get the current wifi connection information.
            _ble_reply_wifi_info();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA:
        {
            // Pill data received from ANT
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT:
        {   
            // Pill heartbeat received from ANT
        }
        break;
    }
}