#include "ble_proto.h"
#include "ble_cmd.h"
#include "wlan.h"

#include "uartstdio.h"

#include "FreeRTOS.h"
#define malloc pvPortMalloc
#define free vPortFree


bool set_wifi(const char* ssid, char* password)
{
    SlSecParams_t secParams;
    int security_type;

    memset(&secParams, 0, sizeof(SlSecParams_t));

    for(security_type = SL_SEC_TYPE_OPEN; security_type < SL_SEC_TYPE_P2P_PIN_AUTO; security_type++)
    {
        secParams.Key = (signed char*)password;
        secParams.KeyLen = password == NULL ? 0 : strlen(password);
        secParams.Type = security_type;

        SlSecParams_t* secParamsPtr = security_type == SL_SEC_TYPE_OPEN ? NULL : &secParams;

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
    int8_t*  name = malloc(32);  // due to wlan.h
    if(!name)
    {
        UARTprintf("Not enough memory.\r\n");
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
        UARTprintf("Get wifi endpoint failed, error %d.\r\n", get_ret);
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
            UARTprintf("Not enough memory.\r\n");
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
            UARTprintf("Wifi SSID %s, pswd %s \r\n", ssid, password);
            if(!set_wifi(ssid, (char*)password))
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
    }
}
