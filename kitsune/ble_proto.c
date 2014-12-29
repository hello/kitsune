#include "simplelink.h"
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


#include "kit_assert.h"
#include "stdlib.h"
#include "stdio.h"
#include "networktask.h"
#include "wifi_cmd.h"
#include "led_animations.h"
#include "led_cmd.h"
#include "top_board.h"
#include "kitsune_version.h"
#include "sys_time.h"
#include "sl_sync_include_after_simplelink_header.h"
#include "ustdlib.h"

typedef void(*task_routine_t)(void*);
typedef enum {
	SENSE_PAIRING_MODE = 0,
	SENSE_ON_BOARDING_MODE,
	SENSE_NORMAL_MODE
} sense_mode_t;

typedef enum {
	LED_BUSY = 0,
	LED_TRIPPY,
	LED_OFF
}led_mode_t;

static struct {
	int a;
	int r;
	int g;
	int b;
	int delay;
	sense_mode_t ble_mode;
	led_mode_t led_status;
} _self;

static void _led_busy_mode(int a, int r, int g, int b, int delay);
static void _led_normal_mode(int operation_result);

static void _factory_reset(){
    int16_t ret = sl_WlanProfileDel(0xFF);
    if(ret)
    {
        LOGI("Delete all stored endpoint failed, error %d.\n", ret);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }else{
        LOGI("All stored WIFI EP removed.\n");
    }

    ret = sl_WlanDisconnect();
    if(ret == 0){
        LOGI("WIFI disconnected\n");
    }else{
        LOGI("Disconnect WIFI failed, error %d.\n", ret);
    }

    while(wifi_status_get(CONNECT))
    {
    	LOGI("Waiting disconnect...\n");
    	vTaskDelay(1000);
    }

	nwp_reset();

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
    wifi_status_set(SCANNING, false);
    
    //Cmd_led(0,0);
    //play_led_progress_bar(30,30,0,0,portMAX_DELAY);
    while((scanned_wifi_count = get_wifi_scan_result(wifi_endpoints, MAX_WIFI_EP_PER_SCAN, 3000 * (max_retry - retry_count + 1))) == 0 && --retry_count)
    {
    	//set_led_progress_bar((max_retry - retry_count) * 100 / max_retry);
        LOGI("No wifi scanned, retry times remain %d\n", retry_count);
        vTaskDelay(500);
    }
    //stop_led_animation();
    wifi_status_set(SCANNING, true);

    int i = 0;
    Sl_WlanNetworkEntry_t wifi_endpoints_cp[2] = {0};
    //play_led_progress_bar(0,0,30,0,portMAX_DELAY);
    MorpheusCommand reply_command = {0};

    for(i = 0; i < scanned_wifi_count; i++)
    {
        wifi_endpoints_cp[0] = wifi_endpoints[i];

		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN;
		reply_command.wifi_scan_result.arg = wifi_endpoints_cp;
		ble_send_protobuf(&reply_command);
		//set_led_progress_bar(i * 100 / scanned_wifi_count);
        vTaskDelay(1000);  // This number must be long enough so the BLE can get the data transmit to phone
        memset(&reply_command, 0, sizeof(reply_command));
    }
    //stop_led_animation();

    reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_STOP_WIFISCAN;
	ble_send_protobuf(&reply_command);
	LOGI(">>>>>>Send WIFI scan results done<<<<<<\n");

}


static bool _set_wifi(const char* ssid, const char* password, int security_type)
{
    int connection_ret;

    uint8_t max_retry = 10;
    uint8_t retry_count = max_retry;

	//play_led_progress_bar(0xFF, 128, 0, 128,portMAX_DELAY);
    while((connection_ret = connect_wifi(ssid, password, security_type)) == 0 && --retry_count)
    {
        //Cmd_led(0,0);
        LOGI("Failed to connect, retry times remain %d\n", retry_count);
        //set_led_progress_bar((max_retry - retry_count ) * 100 / max_retry);
        vTaskDelay(2000);
    }
    //stop_led_animation();

    if(!connection_ret)
    {
		LOGI("Tried all wifi ep, all failed to connect\n");
        ble_reply_protobuf_error(ErrorType_WLAN_CONNECTION_ERROR);
        //led_set_color(0xFF, LED_MAX,0,0,1,1,20,0);
		return 0;
    }else{
		uint8_t wait_time = 5;

		wifi_status_set(CONNECTING, false);
		//play_led_progress_bar(30,30,0,0,portMAX_DELAY);
		while(--wait_time && (!wifi_status_get(HAS_IP)))
		{
            if(!wifi_status_get(CONNECTING))  // This state will be triggered magically by SL_WLAN_CONNECTION_FAILED_EVENT event
            {
            	LOGI("Connection failed!\n");
                break;
            }
			//set_led_progress_bar((10 - wait_time ) * 100 / 10);
			LOGI("Retrieving IP address...\n");
			vTaskDelay(4000);
		}
		//stop_led_animation();

		if(!wifi_status_get(HAS_IP))
		{
			// This is the magical NWP reset problem...
			// we either get an SL_WLAN_CONNECTION_FAILED_EVENT event, or
			// no response, do a NWP reset based on the connection pattern
			// sugguest here: http://e2e.ti.com/support/wireless_connectivity/f/968/p/361673/1273699.aspx
			LOGI("Cannot retrieve IP address, try NWP reset.");
			//led_set_color(0xFF, LED_MAX, 0x66, 0, 1, 0, 15, 0);  // Tell the user we are going to fire the bomb.

			nwp_reset();

			wait_time = 10;
			while(--wait_time && (!wifi_status_get(HAS_IP)))
			{
				vTaskDelay(1000);
			}

			if(wifi_status_get(HAS_IP))
			{
				LOGI("Connection success by NWP reset.");
				//led_set_color(0xFF, LED_MAX, 0x66, 0, 0, 1, 15, 0);
			}else{
				if(wifi_status_get(CONNECTING))
				{
					ble_reply_protobuf_error(ErrorType_FAIL_TO_OBTAIN_IP);
				}else{
					ble_reply_protobuf_error(ErrorType_NO_ENDPOINT_IN_RANGE);
				}
				//led_set_color(0xFF, LED_MAX,0,0,1,1,20,0);
				return 0;
			}
		}
    }

    MorpheusCommand reply_command;
    memset(&reply_command, 0, sizeof(reply_command));
    reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT;
    reply_command.wifiSSID.arg = (void*)ssid;

    LOGI("Connection attempt issued.\n");
    ble_send_protobuf(&reply_command);
    //led_set_color(0xFF, 0,LED_MAX,0,1,1,20,0);
    return 1;
}
static void _reply_sync_device_id()
{

	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type =
			MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID;

	reply_command.has_firmwareVersion = true;
	reply_command.firmwareVersion = FIRMWARE_VERSION_INTERNAL;

	ble_send_protobuf(&reply_command);

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
			usnprintf(&device_id[i * 2], 3, "%02X", mac[i]);  //assert( itoa( mac[i], device_id+i*2, 16 ) == 2 );
		}

		LOGI("Morpheus device id: %s\n", device_id);
		MorpheusCommand reply_command;
		memset(&reply_command, 0, sizeof(reply_command));
		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;

		reply_command.has_firmwareVersion = true;
		reply_command.firmwareVersion = FIRMWARE_VERSION_INTERNAL;

		ble_send_protobuf(&reply_command);

    }else{
        LOGI("Get Mac address failed, error %d.\n", ret);
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

    reply_command.has_wifi_connection_state = true;
    reply_command.wifi_connection_state = wifi_connection_state_NO_WLAN_CONNECTED;

    if(wifi_status_get(CONNECTING)){
        reply_command.wifi_connection_state = wifi_connection_state_WLAN_CONNECTING;
    }

    if(wifi_status_get(CONNECT)){
        reply_command.wifi_connection_state = wifi_connection_state_WLAN_CONNECTED;
    }

    if(wifi_status_get(HAS_IP))
    {
        reply_command.wifi_connection_state = wifi_connection_state_IP_RETRIEVED;
    }

	ble_send_protobuf(&reply_command);
}

#include "wifi_cmd.h"
extern xQueueHandle pill_queue;

static void _process_encrypted_pill_data( MorpheusCommand* command)
{
    if( command->has_pill_data ) {
    	if(!has_good_time())
    	{
    		LOGI("Device not initialized!\n");
    		return;
    	}
    	uint32_t timestamp = get_time();
        command->pill_data.timestamp = timestamp;  // attach timestamp, so we don't need to worry about the sending time
        xQueueSend(pill_queue, &command->pill_data, 10);

        if(command->pill_data.has_motion_data_entrypted)
        {
            LOGI("PILL DATA FROM ID: %s, length: %d\n", command->pill_data.device_id,
                command->pill_data.motion_data_entrypted.size);
        }else{
            LOGI("Bug: No encrypted data!\n");
        }
    }
}

static void _process_pill_heartbeat( MorpheusCommand* command)
{
	// Pill heartbeat received from ANT
    if( command->has_pill_data ) {
		if(!has_good_time())
		{
			LOGI("Device not initialized!\n");
			return;
		}
		uint32_t timestamp = get_time();
		command->pill_data.timestamp = timestamp;  // attach timestamp, so we don't need to worry about the sending time
        xQueueSend(pill_queue, &command->pill_data, 10);
        LOGI("PILL HEARBEAT %s\n", command->pill_data.device_id);

        if (command->pill_data.has_battery_level) {
            LOGI("PILL BATTERY %d\n", command->pill_data.battery_level);
        }
        if (command->pill_data.has_uptime) {
            LOGI("PILL UPTIME %d\n", command->pill_data.uptime);
        }

        if (command->pill_data.has_firmware_version) {
            LOGI("PILL FirmwareVersion %d\n", command->pill_data.firmware_version);
        }
    }
	
}

static void _send_response_to_ble(const char* buffer, size_t len)
{
    const char* header_content_len = "Content-Length: ";
    char * content = strstr(buffer, "\r\n\r\n") + 4;
    char * len_str = strstr(buffer, header_content_len) + strlen(header_content_len);
    if (len_str == NULL) {
        LOGI("Invalid response, Content-Length header not found.\n");
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }

    if (http_response_ok(buffer) != 1) {
        LOGI("Invalid response, %s endpoint return failure.\n", PILL_REGISTER_ENDPOINT);
        ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        return;
    }

    int content_len = atoi(len_str);
    LOGI("Content length %d\n", content_len);
    
    MorpheusCommand response;
    memset(&response, 0, sizeof(response));
    ble_proto_assign_decode_funcs(&response);

    if(decode_rx_data_pb((unsigned char*)content, content_len, MorpheusCommand_fields, &response) == 0)
    {
    	//PANG says: DO NOT EVER REMOVE THIS FUNCTION, ALTHOUGH IT MAKES NO SENSE WHY WE NEED THIS
    	ble_proto_remove_decode_funcs(&response);
    	ble_send_protobuf(&response);
    }else{
    	LOGI("Invalid response, protobuf decryption & decode failed.\n");
    	ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
    }

    ble_proto_free_command(&response);
}

static int _pair_device( MorpheusCommand* command, int is_morpheus)
{
	char response_buffer[256] = {0};
	int ret;
	if(NULL == command->accountId.arg || NULL == command->deviceId.arg){
		LOGI("*******Missing fields\n");
		ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
	}else{

		ble_proto_assign_encode_funcs(command);
		// TODO: Figure out why always get -1 when this is the 1st request
		// after the IPv4 retrieved.

		ret = NetworkTask_SynchronousSendProtobuf(
				DATA_SERVER,
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
			return 1;
		}else{
			LOGI("Pairing request failed, error %d\n", ret);
			ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
		}
	}

	return 0; // failure
}

void _led_busy_mode(int a, int r, int g, int b, int delay)
{
	LOGI("LED BUSY\n");
	_self.a = a;
	_self.r = r;
	_self.g = g;
	_self.b = b;
	_self.delay = delay;

	if(_self.led_status == LED_TRIPPY)
	{
		stop_led_animation();
		led_set_color(_self.a, _self.r, _self.g, _self.b, 1, 1, _self.delay, 1);  // reset the state
		vTaskDelay(_self.delay * (12 + 1));
	}

	_self.led_status = LED_BUSY;
	led_set_color(_self.a, _self.r, _self.g, _self.b, 1, 0, _self.delay, 1);
}

static void _led_normal_mode(int operation_result)
{
	LOGI("LED NORMAL\n");

	if(_self.led_status == LED_BUSY)
	{
		// Stop rolling
		if(operation_result)
		{
			led_set_color(0xFF, LED_MAX, LED_MAX, LED_MAX, 1, 1, 18, 0);
			vTaskDelay(200 * (12 + 1));
		}else{
			led_set_color(_self.a, _self.r, _self.g, _self.b, 1, 1, _self.delay, 1);
			vTaskDelay(_self.delay * (12 + 1));
		}
	}


	switch(_self.ble_mode)
	{
	case SENSE_PAIRING_MODE:
	case SENSE_ON_BOARDING_MODE:
		if(_self.led_status != LED_TRIPPY)
		{
			play_led_trippy(portMAX_DELAY);
		}
		_self.led_status = LED_TRIPPY;
		break;
	case SENSE_NORMAL_MODE:
		if(_self.led_status == LED_TRIPPY)
		{
			stop_led_animation();
		}
		_self.led_status = LED_OFF;

		break;
	}
}

#include "top_board.h"
#include "wifi_cmd.h"
extern uint8_t top_device_id[DEVICE_ID_SZ];
extern volatile bool top_got_device_id; //being bad, this is only for factory

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
            LOGI("Wifi SSID %s, pswd %s \n", ssid, password);

            int sec_type = SL_SEC_TYPE_WPA_WPA2;
            if(command->has_security_type)
            {
            	sec_type = command->security_type == wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA2 ? SL_SEC_TYPE_WPA_WPA2 : command->security_type;
            }

            _led_busy_mode(0xFF, 128, 0, 128, 18);
            int result = _set_wifi(ssid, (char*)password, sec_type);
            _led_normal_mode(result);

        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
        	_self.ble_mode = SENSE_PAIRING_MODE;
            // Light up LEDs?
			_led_normal_mode(0);
            LOGI( "PAIRING MODE \n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:  // Just for testing
		{
			_self.ble_mode = SENSE_NORMAL_MODE;
			_led_normal_mode(0);
			LOGI( "NORMAL MODE \n");
		}
		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        {
            // Get the current wifi connection information.
            _ble_reply_wifi_info();
            LOGI( "GET_WIFI\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_CONNECTED:
        {
        	LOGI("PHONE CONNECTED\n");
        	_led_busy_mode(0xFF, 128, 0, 128, 18);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_BONDED:
        {
        	_led_normal_mode(1);
        	LOGI("PHONE BONDED\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        {
            // Get morpheus device id request from Nordic
            LOGI("GET DEVICE ID\n");
            top_board_notify_boot_complete();
            _self.led_status = LED_OFF;  // init led status
            _reply_device_id();
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA: 
        {
    		// Pill data received from ANT
        	if(command->has_pill_data){
        		LOGI("PILL DATA %s\n", command->pill_data.device_id);
        	}else{
        		LOGI("You may have a bug in the pill\n");
        	}
    		_process_encrypted_pill_data(command);
    	}
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT: 
        {
            LOGI("PILL HEARTBEAT\n");
    		_process_pill_heartbeat(command);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL:
        {
            LOGI("PAIR PILL\n");
            _self.ble_mode = SENSE_ON_BOARDING_MODE;
            _led_busy_mode(0xFF, 128, 0, 128, 18);
            int result = _pair_device(command, 0);
            _led_normal_mode(result);
            
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
        {
        	_self.ble_mode = SENSE_ON_BOARDING_MODE;
        	_led_busy_mode(0xFF, 128, 0, 128, 18);
            LOGI("PAIR SENSE\n");
            int result = _pair_device(command, 1);
            _led_normal_mode(result);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET:
        {
            LOGI("FACTORY RESET\n");
            _led_busy_mode(0xFF, 128, 0, 128, 18);
            _factory_reset();
            _led_normal_mode(1);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN:
        {
            LOGI("WIFI Scan request\n");
            _led_busy_mode(0xFF, 128, 0, 128, 18);
            _reply_wifi_scan_result();
            _led_normal_mode(0);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES:
        {
            LOGI("PILL SHAKES\n");
            _led_busy_mode(0xFF, 128, 0, 128, 18);

        	vTaskDelay(18 *4 * (12 + 1));
            _led_normal_mode(0);
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
        {
        	int i;
        	if(command->deviceId.arg){
        		const char * device_id_str = command->deviceId.arg;
        		for(i=0;i<DEVICE_ID_SZ;++i) {
        			char num[3] = {0};
        			memcpy( num, device_id_str+i*2, 2);
        			top_device_id[i] = strtol( num, NULL, 16 );
        		}
        		LOGI("got id from top %x:%x:%x:%x:%x:%x:%x:%x\n",
        				top_device_id[0],top_device_id[1],top_device_id[2],
        				top_device_id[3],top_device_id[4],top_device_id[5],
        				top_device_id[6],top_device_id[7]);
        		top_got_device_id = true;
        		_reply_sync_device_id();
        	}else{
        		LOGI("device id fail from top\n");
        	}
    	}
        break;
	}
    return true;
}
