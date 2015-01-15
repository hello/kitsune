#include "simplelink.h"
#include "ble_proto.h"
#include "proto_utils.h"
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
#include "audiotask.h"
#include "wifi_cmd.h"
#include "led_animations.h"
#include "led_cmd.h"
#include "top_board.h"
#include "kitsune_version.h"
#include "sys_time.h"
#include "sl_sync_include_after_simplelink_header.h"
#include "ustdlib.h"
#include "pill_settings.h"
#include "audiohelper.h"
#include "audiotask.h"

typedef void(*task_routine_t)(void*);

typedef enum {
	LED_BUSY = 0,
	LED_TRIPPY,
	LED_OFF
}led_mode_t;

typedef enum {
    BLE_UNKNOWN = 0,
    BLE_PAIRING,
    BLE_NORMAL
} ble_mode_t;

static struct {
	uint8_t argb[4];
	int delay;
	uint32_t last_hold_time;
	led_mode_t led_status;
    ble_mode_t ble_status;
} _self;

static uint8_t _wifi_read_index;
static Sl_WlanNetworkEntry_t _wifi_endpoints[MAX_WIFI_EP_PER_SCAN];

static void _ble_reply_command_with_type(MorpheusCommand_CommandType type)
{
	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = type;
	ble_send_protobuf(&reply_command);
}

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

    pill_settings_reset_all();
    nwp_reset();
    deleteFilesInDir(USER_DIR);
	_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET);

}

static int _scan_wifi()
{
	memset(_wifi_endpoints, 0, sizeof(_wifi_endpoints));
	int scanned_wifi_count = 0;
	_wifi_read_index = 0;

	uint8_t max_retry = 3;
	uint8_t retry_count = max_retry;
	wifi_status_set(SCANNING, false);

	while((scanned_wifi_count = get_wifi_scan_result(_wifi_endpoints, MAX_WIFI_EP_PER_SCAN, 3000 * (max_retry - retry_count + 1))) == 0 && --retry_count)
	{
		LOGI("No wifi scanned, retry times remain %d\n", retry_count);
		vTaskDelay(500);
	}

	wifi_status_set(SCANNING, true);
	return scanned_wifi_count;
}

static void _reply_next_wifi_ap()
{
	if(_wifi_read_index == MAX_WIFI_EP_PER_SCAN - 1){
		_wifi_read_index = 0;
	}

	MorpheusCommand reply_command = {0};
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_NEXT_WIFI_AP;
	reply_command.wifi_scan_result.arg = &_wifi_endpoints[_wifi_read_index++];
	ble_send_protobuf(&reply_command);
}

static void _reply_wifi_scan_result()
{
    int scanned_wifi_count = _scan_wifi();
    int i = 0;
    MorpheusCommand reply_command = {0};

    for(i = 0; i < scanned_wifi_count; i++)
    {
		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN;
		reply_command.wifi_scan_result.arg = &_wifi_endpoints[i];
		ble_send_protobuf(&reply_command);
        vTaskDelay(1000);  // This number must be long enough so the BLE can get the data transmit to phone
        memset(&reply_command, 0, sizeof(reply_command));
    }

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

void sample_sensor_data(periodic_data* data);
static int _force_data_push()
{
    if(!wait_for_time(10))
    {
    	ble_reply_protobuf_error(ErrorType_NETWORK_ERROR);
		return 0;
    }

    periodic_data* data = pvPortMalloc(sizeof(periodic_data));  // Let's put this in the heap, we don't use it all the time
	if(!data)
	{
		ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
		return 0;
	}
    memset(data, 0, sizeof(periodic_data));
    sample_sensor_data(data);

    periodic_data_to_encode periodicdata;
    periodicdata.num_data = 1;
    periodicdata.data = data;

    batched_periodic_data data_batched = {0};
    pack_batched_periodic_data(&data_batched, &periodicdata);

    uint8_t retry = 3;
    int ret = 0;
    while ((ret = send_periodic_data(&data_batched)) != 0) {
        LOGI("Retry\n");
        vTaskDelay(1 * configTICK_RATE_HZ);
        --retry;
        if(!retry)
        {
            LOGI("Force post data failed\n");
            break;
        }
    }
    vPortFree(data);
    return ret;
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

void ble_proto_led_init()
{
	_self.led_status = LED_OFF;
	led_set_color_sync(0xFF, LED_MAX, LED_MAX, LED_MAX, 1, 1, 18, 0, 1);
}

void ble_proto_led_busy_mode(uint8_t a, uint8_t r, uint8_t g, uint8_t b, int delay)
{
	LOGI("LED BUSY\n");
	_self.argb[0] = a;
	_self.argb[1] = r;
	_self.argb[2] = g;
	_self.argb[3] = b;
	_self.delay = delay;

	if(_self.led_status == LED_TRIPPY)
	{
		ble_proto_led_fade_out(0);
	}

	if(_self.led_status == LED_BUSY && _self.argb[0] == a && _self.argb[1] == r && _self.argb[2] == g && _self.argb[3] == g)
	{
		return;
	}

	_self.led_status = LED_BUSY;
	led_set_color(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 1, 0, _self.delay, 1);
}

void ble_proto_led_flash(int a, int r, int g, int b, int delay, int time)
{
	LOGI("LED ROLL ONCE\n");
	_self.argb[0] = a;
	_self.argb[1] = r;
	_self.argb[2] = g;
	_self.argb[3] = b;
	_self.delay = delay;
	int i = 0;
	if(_self.led_status == LED_TRIPPY)
	{
		ble_proto_led_fade_out(0);

		_self.led_status = LED_BUSY;
		for(i = 0; i < time; i++)
		{
			led_set_color_sync(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 1, 1, _self.delay, 0, 1);
		}
		_self.led_status = LED_OFF;
		ble_proto_led_fade_in_trippy();
	}else if(_self.led_status == LED_OFF){
		_self.led_status = LED_BUSY;
		for(i = 0; i < time; i++)
		{
			led_set_color_sync(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 1, 1, _self.delay, 0, 1);
		}
		_self.led_status = LED_OFF;
	}

}

void ble_proto_led_fade_in_trippy(){
	switch(_self.led_status)
	{
	case LED_BUSY:
		led_set_color_sync(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 0, 1, 18, 0, 1);
		play_led_trippy(portMAX_DELAY);

		break;
	case LED_TRIPPY:
		break;
	case LED_OFF:
		//led_set_color(_self.a, _self.r, _self.g, _self.b, 1, 0, 18, 0);
		play_led_trippy(portMAX_DELAY);
		break;
	}

	_self.led_status = LED_TRIPPY;
}

void ble_proto_led_fade_out(bool operation_result){
	switch(_self.led_status)
	{
	case LED_BUSY:
        if(operation_result)
        {
            led_set_color_sync(0xFF, LED_MAX, LED_MAX, LED_MAX, 0, 1, 18, 0, 1);
        }else{
        	led_set_color_sync(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 0, 1, 18, 1, 1);
        }
		break;
	case LED_TRIPPY:
		stop_led_animation_sync();
		//led_set_color(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 0, 1, 18, 0);

		break;
	case LED_OFF:
		break;
	}

	_self.led_status = LED_OFF;
}

#include "top_board.h"
#include "wifi_cmd.h"
extern uint8_t top_device_id[DEVICE_ID_SZ];
extern volatile bool top_got_device_id; //being bad, this is only for factory
void ble_proto_start_hold()
{
	_self.last_hold_time = xTaskGetTickCount();
    switch(_self.ble_status)
    {
        case BLE_PAIRING:
        {
            // hold to cancel the pairing mode
            LOGI("Back to normal mode\n");
            MorpheusCommand response = {0};
            response.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
            ble_send_protobuf(&response);

        }
        break;
    }
}

void ble_proto_end_hold()
{
	//configTICK_RATE_HZ
	uint32_t current_tick = xTaskGetTickCount();
	if((current_tick - _self.last_hold_time) * (1000 / configTICK_RATE_HZ) > 5000 &&
		(current_tick - _self.last_hold_time) * (1000 / configTICK_RATE_HZ) < 10000 &&
		_self.last_hold_time > 0)
	{
        switch(_self.ble_status)
        {
            case BLE_NORMAL:
            {
        		LOGI("Trigger pairing mode\n");
        		MorpheusCommand response = {0};
        		response.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE;
        		ble_send_protobuf(&response);
        		ble_proto_led_fade_in_trippy();
            }
            break;
        }
	}
	_self.last_hold_time = 0;
}

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

            int result = _set_wifi(ssid, (char*)password, sec_type);

        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
            // Light up LEDs?
			ble_proto_led_fade_in_trippy();
            _self.ble_status = BLE_PAIRING;
            LOGI( "PAIRING MODE \n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:  // Just for testing
		{
			ble_proto_led_fade_out(0);
            _self.ble_status = BLE_NORMAL;
			LOGI( "NORMAL MODE \n");
		}
		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        {
            // Get the current wifi connection information.
            _ble_reply_wifi_info();
            //LOGI( "GET_WIFI\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_CONNECTED:
        {
        	LOGI("PHONE CONNECTED\n");
        	ble_proto_led_busy_mode(0xFF, 128, 0, 128, 18);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_BONDED:
        {
        	ble_proto_led_fade_out(0);
        	LOGI("PHONE BONDED\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        {
            // Get morpheus device id request from Nordic
            LOGI("GET DEVICE ID\n");
            _self.led_status = LED_OFF;  // init led status
            _ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID);

            if(command->has_ble_bond_count)
            {
            	// this command fires before MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID
            	// and it is the 1st command you can get from top.
            	if(!command->ble_bond_count){
            		// If we had ble_bond_count field, boot LED animation can start from here. Visual
            		// delay of device boot can be greatly reduced.
            		ble_proto_led_init();
					ble_proto_led_fade_in_trippy();

					// TODO: Play startup sound. You will only reach here once.
					// Now the hand hover-to-pairing mode will not delete all the bonds
					// when the bond db is full, so you will never get zero after a phone bonds
					// to Sense, unless user do factory reset and power cycle the device.

					vTaskDelay(10);
					{
				    AudioPlaybackDesc_t desc;
				    memset(&desc,0,sizeof(desc));
				    strncpy( desc.file, "/start.raw", strlen("/start.raw") );
				    desc.volume = 57;
				    desc.durationInSeconds = -1;
				    desc.rate = 48000;
				    AudioTask_StartPlayback(&desc);
					}
					vTaskDelay(200);


            	}else{
            		ble_proto_led_fade_out(0);
            	}
            } else {
            	ble_proto_led_init();
            }
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
            //LOGI("PAIR PILL\n");
            int result = _pair_device(command, 0);
            
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
        {
            //LOGI("PAIR SENSE\n");
            int result = _pair_device(command, 1);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET:
        {
            //LOGI("FACTORY RESET\n");
            _factory_reset();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN:
        {
            LOGI("WIFI Scan request\n");
            _reply_wifi_scan_result();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES:
        {
            LOGI("PILL SHAKES\n");
            if(command->deviceId.arg){
				uint32_t color = pill_settings_get_color((const char*)command->deviceId.arg);
				uint8_t* argb = (uint8_t*)&color;

				if(color)
				{
					ble_proto_led_flash(0xFF, argb[1], argb[2], argb[3], 10, 2);
				}else{
					if(_self.led_status == LED_TRIPPY || pill_settings_pill_count() == 0)
					{
						ble_proto_led_flash(0xFF, 0x80, 0x00, 0x80, 10, 2);
					}
				}
            }else{
            	LOGI("Please update topboard, no pill id\n");
            }
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
        {
        	int i;
        	if(command->deviceId.arg){
#ifndef DONT_GET_ID_FROM_TOP
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
#endif
        		_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID);
        		top_board_notify_boot_complete();
        	}else{
        		LOGI("device id fail from top\n");
        	}
    	}
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_BUSY:
    		ble_proto_led_busy_mode(0xFF, 128, 0, 128, 18);
    		_ble_reply_command_with_type(command->type);
    		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_FAILED:
    		ble_proto_led_fade_out(false);
    		_ble_reply_command_with_type(command->type);
    		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_SUCCESS:
            ble_proto_led_fade_out(true);
            _ble_reply_command_with_type(command->type);
            break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_TRIPPY:
    		ble_proto_led_fade_in_trippy();
    		_ble_reply_command_with_type(command->type);
    		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SCAN_WIFI:
    		_scan_wifi();
    		_ble_reply_command_with_type(command->type);
    		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_NEXT_WIFI_AP:
    		_reply_next_wifi_ap();
    		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PUSH_DATA_AFTER_SET_TIMEZONE:
        {
            LOGI("Push data\n");
            if(_force_data_push() != 0)
            {
                ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }else{
                _ble_reply_command_with_type(command->type);
            }
        }
        break;
	}
    return true;
}
