
#include "ble_proto.h"
#include "ble_cmd.h"
#include "wlan.h"

#include "wifi_cmd.h"
#include "uartstdio.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"


#include "assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "networktask.h"
#include "led_animations.h"
#include "led_cmd.h"
#include "top_board.h"

extern unsigned int sl_status;

static void _factory_reset(){
    int16_t ret = sl_WlanProfileDel(0xFF);
    if(ret)
    {
        UARTprintf("Delete all stored endpoint failed, error %d.\n", ret);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }else{
        UARTprintf("All stored WIFI EP removed.\n");
    }

    ret = sl_WlanDisconnect();
    if(ret == 0){
        UARTprintf("WIFI disconnected");
    }else{
        UARTprintf("Disconnect WIFI failed, error %d.\n", ret);
    }

    ret = sl_Stop(2);
    if(ret == 0)
    {
    	sl_Start(NULL, NULL, NULL);
    }else{
    	UARTprintf("NWP reset failed\n");
    }

    MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET;
	ble_send_protobuf(&reply_command);
}

static void _reply_wifi_scan_result()
{
    Sl_WlanNetworkEntry_t wifi_endpoints[MAX_WIFI_EP_PER_SCAN] = {0};
    int scanned_wifi_count = 0;

    uint8_t max_retry = 3;
    uint8_t retry_count = max_retry;
    sl_status |= SCANNING;
    
    //Cmd_led(0,0);
    play_led_progress_bar(30,30,0,0);
    while((scanned_wifi_count = get_wifi_scan_result(wifi_endpoints, MAX_WIFI_EP_PER_SCAN, 3000 * (max_retry - retry_count + 1))) == 0 && --retry_count)
    {
    	set_led_progress_bar((max_retry - retry_count) * 100 / max_retry);
        UARTprintf("No wifi scanned, retry times remain %d\n", retry_count);
        vTaskDelay(500);
    }
    stop_led_animation();
    sl_status &= ~SCANNING;

    int i = 0;
    Sl_WlanNetworkEntry_t wifi_endpoints_cp[2] = {0};
    play_led_progress_bar(0,0,30,0);
    MorpheusCommand reply_command = {0};
    for(i = 0; i < scanned_wifi_count; i++)
    {
        wifi_endpoints_cp[0] = wifi_endpoints[i];

		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN;
		reply_command.wifi_scan_result.arg = wifi_endpoints_cp;
		ble_send_protobuf(&reply_command);
		set_led_progress_bar(i * 100 / scanned_wifi_count);
        vTaskDelay(1000);  // This number must be long enough so the BLE can get the data transmit to phone
        memset(&reply_command, 0, sizeof(reply_command));
    }
    stop_led_animation();

    reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_STOP_WIFISCAN;
	ble_send_protobuf(&reply_command);
	UARTprintf(">>>>>>Send WIFI scan results done<<<<<<\n");

}


static bool _set_wifi(const char* ssid, const char* password, int security_type)
{
    int connection_ret;

    uint8_t max_retry = 10;
    uint8_t retry_count = max_retry;

	play_led_progress_bar(30,30,0,0);
    while((connection_ret = connect_wifi(ssid, password, security_type)) == 0 && --retry_count)
    {
        //Cmd_led(0,0);
        UARTprintf("Failed to connect, retry times remain %d\n", retry_count);
        set_led_progress_bar((max_retry - retry_count ) * 100 / max_retry);
        vTaskDelay(2000);
    }
    stop_led_animation();

    if(!connection_ret)
    {
		UARTprintf("Tried all wifi ep, all failed to connect\n");
        ble_reply_protobuf_error(ErrorType_WLAN_CONNECTION_ERROR);
        led_set_color(0xFF, 30,0,0,1,1,60,0);
		return 0;
    }else{
		uint8_t wait_time = 10;

		sl_status |= CONNECTING;
		play_led_progress_bar(30,30,0,0);
		while(--wait_time && (!(sl_status & HAS_IP)))
		{
			//Cmd_led(0,0);
			set_led_progress_bar((10 - wait_time ) * 100 / 10);
			UARTprintf("Retrieving IP address...\n");
			vTaskDelay(4000);
		}
		stop_led_animation();
		if(!(sl_status & HAS_IP))
		{
			//Cmd_led(0,0);
			UARTprintf("!!WIFI set without network connection.");
            ble_reply_protobuf_error(ErrorType_FAIL_TO_OBTAIN_IP);
            led_set_color(0xFF, 30,0,0,1,1,60,0);
			return 0;
		}
    }

    MorpheusCommand reply_command;
    memset(&reply_command, 0, sizeof(reply_command));
    reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT;
    reply_command.wifiSSID.arg = (void*)ssid;

    UARTprintf("Connection attempt issued.\n");
    ble_send_protobuf(&reply_command);
    led_set_color(0xFF, 0,30,0,1,1,200,0);
    return 1;
}

static void _reply_device_id()
{
    uint8_t mac_len = SL_MAC_ADDR_LEN;
    uint8_t mac[SL_MAC_ADDR_LEN] = {0};

    int32_t ret = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
    if(ret == 0 || ret == SL_ESMALLBUF)  // OK you win: http://e2e.ti.com/support/wireless_connectivity/f/968/p/360573/1279578.aspx#1279578
    {

        char device_id[SL_MAC_ADDR_LEN * 2 + 1] = {0}; //pvPortMalloc(device_id_len);

		uint8_t i = 0;
		for(i = 0; i < SL_MAC_ADDR_LEN; i++){
			snprintf(&device_id[i * 2], 3, "%02X", mac[i]);  //assert( itoa( mac[i], device_id+i*2, 16 ) == 2 );
		}

		UARTprintf("Morpheus device id: %s\n", device_id);
		MorpheusCommand reply_command;
		memset(&reply_command, 0, sizeof(reply_command));
		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;

		reply_command.deviceId.arg = device_id;
		reply_command.has_firmwareVersion = true;
		reply_command.firmwareVersion = FIRMWARE_VERSION_INTERNAL;

		ble_send_protobuf(&reply_command);

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
	uint8_t ssid[MAX_SSID_LEN] = {0};
	wifi_get_connected_ssid(ssid, sizeof(ssid));

	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT;
	size_t len = strlen((char*)ssid) + 1;
	if(len - 1 > 0)
	{
		reply_command.wifiSSID.arg = ssid;
	}
	ble_send_protobuf(&reply_command);
}

#include "wifi_cmd.h"
extern xQueueHandle pill_queue;
unsigned long get_time();

static void _process_encrypted_pill_data( MorpheusCommand* command)
{
    if( command->has_pill_data ) {
        command->pill_data.timestamp = get_time();  // attach timestamp, so we don't need to worry about the sending time
        xQueueSend(pill_queue, &command->pill_data, 10);

        if(command->pill_data.has_motion_data_entrypted)
        {
            UARTprintf("PILL DATA FROM ID: %s, length: %d\n", command->pill_data.device_id,
                command->pill_data.motion_data_entrypted.size);
        }else{
            UARTprintf("Bug: No encrypted data!\n");
        }
    }
}

static void _process_pill_heartbeat( MorpheusCommand* command)
{
	// Pill heartbeat received from ANT
    if( command->has_pill_data ) {
        command->pill_data.timestamp = get_time();
        xQueueSend(pill_queue, &command->pill_data, 10);
        UARTprintf("PILL HEARBEAT %s\n", command->pill_data.device_id);

        if (command->pill_data.has_battery_level) {
            UARTprintf("PILL BATTERY %d\n", command->pill_data.battery_level);
        }
        if (command->pill_data.has_uptime) {
            UARTprintf("PILL UPTIME %d\n", command->pill_data.uptime);
        }

        if (command->pill_data.has_firmware_version) {
            UARTprintf("PILL FirmwareVersion %d\n", command->pill_data.firmware_version);
        }
    }
	
}

static void _send_response_to_ble(const char* buffer, size_t len)
{
    const char* header_content_len = "Content-Length: ";
    char * content = strstr(buffer, "\r\n\r\n") + 4;
    char * len_str = strstr(buffer, header_content_len) + strlen(header_content_len);
    if (len_str == NULL) {
        UARTprintf("Invalid response, Content-Length header not found.\n");
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }

    if (http_response_ok(buffer) != 1) {
        UARTprintf("Invalid response, %s endpoint return failure.\n", PILL_REGISTER_ENDPOINT);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }

    int content_len = atoi(len_str);
    UARTprintf("Content length %d\n", content_len);
    
    MorpheusCommand response;
    memset(&response, 0, sizeof(response));
    ble_proto_assign_decode_funcs(&response);

    if(decode_rx_data_pb((unsigned char*)content, content_len, MorpheusCommand_fields, &response) == 0)
    {
    	//PANG says: DO NOT EVER REMOVE THIS FUNCTION, ALTHOUGH IT MAKES NO SENSE WHY WE NEED THIS
    	ble_proto_remove_decode_funcs(&response);
    	ble_send_protobuf(&response);
    }else{
    	UARTprintf("Invalid response, protobuf decryption & decode failed.\n");
    	ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
    }

    ble_proto_free_command(&response);
}

static void _pair_device( MorpheusCommand* command, int is_morpheus)
{
	char response_buffer[256] = {0};
	int ret;
	if(NULL == command->accountId.arg || NULL == command->deviceId.arg){
		UARTprintf("*******Missing fields\n");
		ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
	}else{

		ble_proto_assign_encode_funcs(command);
		// TODO: Figure out why always get -1 when this is the 1st request
		// after the IPv4 retrieved.

		ret = NetworkTask_SynchronousSendProtobuf(
				is_morpheus == 1 ? MORPHEUS_REGISTER_ENDPOINT : PILL_REGISTER_ENDPOINT,
				response_buffer,
				sizeof(response_buffer),
				MorpheusCommand_fields,
				command,
				5000);

		// All the args are in stack, don't need to do protobuf free.

		if(ret == 0)
		{
			_send_response_to_ble(response_buffer, sizeof(response_buffer));
		}else{
			UARTprintf("Pairing request failed, error %d\n", ret);
			ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
		}
	}
}

#include "top_board.h"


bool on_ble_protobuf_command(MorpheusCommand* command)
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

            int sec_type = SL_SEC_TYPE_WPA_WPA2;
            if(command->has_security_type)
            {
            	sec_type = command->security_type == wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA2 ? SL_SEC_TYPE_WPA_WPA2 : command->security_type;
            }

            _set_wifi(ssid, (char*)password, sec_type);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
            // Light up LEDs?
        	led_set_color(0xFF, 0, 0, 50, 1, 1, 18, 0); //blue
            UARTprintf( "PAIRING MODE \n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        {
            // Get the current wifi connection information.
            _ble_reply_wifi_info();
            UARTprintf( "GET_WIFI\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        {
            // Get morpheus device id request from Nordic
            UARTprintf("GET DEVICE ID\n");
            top_board_notify_boot_complete();
            _reply_device_id();
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA: 
        {
    		// Pill data received from ANT
        	if(command->has_pill_data){
        		UARTprintf("PILL DATA %s\n", command->pill_data.device_id);
        	}else{
        		UARTprintf("You may have a bug in the pill\n");
        	}
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
            led_set_color(0xFF, 0, 0, 50, 1, 1, 18, 1); //blue
            _pair_device(command, 0);
            
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
        {
            UARTprintf("PAIR SENSE\n");
            _pair_device(command, 1);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET:
        {
            UARTprintf("FACTORY RESET\n");
            _factory_reset();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN:
        {
            UARTprintf("WIFI Scan request\n");
            _reply_wifi_scan_result();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES:
        {
            UARTprintf("PILL SHAKES\n");
            led_set_color(0xFF, 0, 0, 50, 1, 1, 18, 1); //blue
        }
        break;
	}
    return true;
}
