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
#include "hlo_net_tools.h"

typedef void(*task_routine_t)(void*);

typedef enum {
	LED_BUSY = 0,
	LED_TRIPPY,
	LED_OFF
}led_mode_t;

static struct {
	uint8_t argb[4];
	int delay;
	uint32_t last_hold_time;
    ble_mode_t ble_status;
    xSemaphoreHandle smphr;
} _self;

static int _scanned_wifi_count = 0;
static Sl_WlanNetworkEntry_t _wifi_endpoints[MAX_WIFI_EP_PER_SCAN];
static xSemaphoreHandle _wifi_smphr;
static hlo_future_t * scan_results;

ble_mode_t get_ble_mode() {
	ble_mode_t status;
	xSemaphoreTake( _self.smphr, portMAX_DELAY );
	status = _self.ble_status;
	xSemaphoreGive( _self.smphr );
	return status;
}
static void set_ble_mode(ble_mode_t status) {
	xSemaphoreTake( _self.smphr, portMAX_DELAY );
	_self.ble_status = status;
	xSemaphoreGive( _self.smphr );
}

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

    reset_default_antenna();
    pill_settings_reset_all();
    nwp_reset();
    deleteFilesInDir(USER_DIR);
	_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET);

}

void ble_proto_init() {
	vSemaphoreCreateBinary(_self.smphr);
	vSemaphoreCreateBinary(_wifi_smphr);
	set_ble_mode(BLE_NORMAL);
}

static void _reply_wifi_scan_result()
{
    int i = 0;
    MorpheusCommand reply_command = {0};
    _scanned_wifi_count = hlo_future_read(scan_results,_wifi_endpoints,sizeof(_wifi_endpoints), 10000);
    for(i = 0; i < _scanned_wifi_count; i++)
    {
		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN;
		reply_command.wifi_scan_result.arg = &_wifi_endpoints[i];
		ble_send_protobuf(&reply_command);
        vTaskDelay(250);  // This number must be long enough so the BLE can get the data transmit to phone
        memset(&reply_command, 0, sizeof(reply_command));
    }
	_scanned_wifi_count = 0;
    reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_STOP_WIFISCAN;
	ble_send_protobuf(&reply_command);
	LOGI(">>>>>>Send WIFI scan results done<<<<<<\n");

}

static bool _set_wifi(const char* ssid, const char* password, int security_type, int version)
{
    int connection_ret, i;

    uint8_t max_retry = 10;
    uint8_t retry_count = max_retry;

	xSemaphoreTake(_wifi_smphr, portMAX_DELAY);
    for(i=0;i<MAX_WIFI_EP_PER_SCAN;++i) {
    	if( !strcmp( (char*)_wifi_endpoints[i].ssid, ssid ) ) {
    		antsel(_wifi_endpoints[i].reserved[0]);
    		save_default_antenna( _wifi_endpoints[i].reserved[0] );
    		break;
    	}
    }
	xSemaphoreGive(_wifi_smphr);

	//play_led_progress_bar(0xFF, 128, 0, 128,portMAX_DELAY);
    while((connection_ret = connect_wifi(ssid, password, security_type, version)) == 0 && --retry_count)
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

		// Wait until the disconnect event happen. If we have a WIFI connection already
		// and the user enter the wrong password in the next WIFI setup, it will go straight to success
		// without returning error.
		vTaskDelay(1000);

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

static void _morpheus_command_reply(const NetworkResponse_t * response,
		char * reply_buf, int reply_sz, void * context) {
	MorpheusCommand reply;
	bool * success = (bool*)context;
	memset(&reply, 0, sizeof(reply));
	ble_proto_assign_decode_funcs(&reply);
    if( response->success && validate_signatures((char*)reply_buf, MorpheusCommand_fields, &reply) == 0) {
		ble_proto_remove_decode_funcs(&reply);
		ble_send_protobuf(&reply);
    	LOGF("signature validated\r\n");
    	if( success ) {
    		*success = true;
    	}
    } else {
        LOGF("signature validation fail\r\n");
    	if( success ) {
    		*success = false;
    	}
    }
    ble_proto_free_command(&reply);
}

void save_account_id( char * acct );
int force_data_push();
static int _pair_device( MorpheusCommand* command, int is_morpheus)
{
	bool success = false;
	if(NULL == command->accountId.arg || NULL == command->deviceId.arg){
		LOGI("*******Missing fields\n");
		ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
	}else{
		save_account_id( command->accountId.arg );

		ble_proto_assign_encode_funcs(command);

	    bool  ret = NetworkTask_SendProtobuf( true,
					DATA_SERVER,
					is_morpheus == 1 ? MORPHEUS_REGISTER_ENDPOINT : PILL_REGISTER_ENDPOINT,
					MorpheusCommand_fields,
					command,
					30000,
					_morpheus_command_reply, &success);

		// All the args are in stack, don't need to do protobuf free.
		if(!is_morpheus) {
			vTaskDelay(1000);
			force_data_push();
		}
		if(!ret || !success )
		{
			LOGI("Pairing request failed, error %d\n", ret);
			ble_reply_protobuf_error(ErrorType_NETWORK_ERROR);
		}
	}

	return 0; // failure
}

void ble_proto_led_init()
{
	play_led_animation_solid(LED_MAX, LED_MAX, LED_MAX,LED_MAX,1, 33);
}


void ble_proto_led_busy_mode(uint8_t a, uint8_t r, uint8_t g, uint8_t b, int delay)
{
	LOGI("LED BUSY\n");
	_self.argb[0] = a;
	_self.argb[1] = r;
	_self.argb[2] = g;
	_self.argb[3] = b;
	_self.delay = delay;

	ble_proto_led_fade_out(false);
	play_led_wheel(a,r,g,b,0,delay);
}

void ble_proto_led_flash(int a, int r, int g, int b, int delay)
{
	static uint32_t last = 0;
	if( xTaskGetTickCount() - last < 3000 ) {
		return;
	}
	last = xTaskGetTickCount();

	LOGI("LED ROLL ONCE\n");
	_self.argb[0] = a;
	_self.argb[1] = r;
	_self.argb[2] = g;
	_self.argb[3] = b;
	_self.delay = delay;

	ANIMATE_BLOCKING(play_led_animation_solid(a,r,g,b,2,18), 4000);
}
extern volatile bool provisioning_mode;

void ble_proto_led_fade_in_trippy(){
	uint8_t trippy_base[3] = {60, 25, 90};
	play_led_trippy(trippy_base, trippy_base, portMAX_DELAY, 30);
}

void ble_proto_led_fade_out(bool operation_result){
	stop_led_animation( 10000, 18 );

	if(operation_result) {
		ANIMATE_BLOCKING(play_led_animation_solid(LED_MAX,LED_MAX,LED_MAX,LED_MAX,1,11), 4000);
	}
}

void ble_proto_charger_detect_begin () {
MorpheusCommand response = {0};
response.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
ble_send_protobuf(&response);
}

#include "top_board.h"
#include "wifi_cmd.h"
extern uint8_t top_device_id[DEVICE_ID_SZ];
extern volatile bool top_got_device_id; //being bad, this is only for factory
void ble_proto_start_hold()
{
	_self.last_hold_time = xTaskGetTickCount();
    switch(get_ble_mode())
    {
        case BLE_PAIRING:
        {
            // hold to cancel the pairing mode
            LOGI("Back to normal mode\n");
            MorpheusCommand response = {0};
            response.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
            ble_send_protobuf(&response);

			analytics_event( "{ble: normal}" );
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
		if (get_ble_mode() != BLE_PAIRING) {
			LOGI("Trigger pairing mode\n");
			MorpheusCommand response = { 0 };
			response.type =
					MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE;
			ble_send_protobuf(&response);
			ble_proto_led_fade_in_trippy();
			analytics_event( "{ble: pairing}" );
		}
	}
	_self.last_hold_time = 0;
}

static void play_startup_sound() {
	// TODO: Play startup sound. You will only reach here once.
	// Now the hand hover-to-pairing mode will not delete all the bonds
	// when the bond db is full, so you will never get zero after a phone bonds
	// to Sense, unless user do factory reset and power cycle the device.

	vTaskDelay(10);
	{
		AudioPlaybackDesc_t desc;
		memset(&desc, 0, sizeof(desc));
		strncpy(desc.file, "/ringtone/star003.raw", strlen("/ringtone/star003.raw"));
		desc.volume = 57;
		desc.durationInSeconds = -1;
		desc.rate = 48000;
		desc.fade_in_ms = 0;
		desc.fade_out_ms = 0;
		AudioTask_StartPlayback(&desc);
	}
	vTaskDelay(175);
}

#include "crypto.h"
extern volatile bool booted;
extern volatile bool provisioning_mode;
extern uint8_t aes_key[AES_BLOCKSIZE + 1];
int save_device_id( uint8_t * device_id );
int save_aes( uint8_t * key ) ;

bool on_ble_protobuf_command(MorpheusCommand* command)
{
	switch(command->type)
	{
		case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
		{
			LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID\n");
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
			    save_device_id(top_device_id);
				_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID);
				top_board_notify_boot_complete();
				set_ble_mode(BLE_NORMAL);

				if(command->has_aes_key && should_burn_top_key()){
					save_aes(command->aes_key.bytes);
					LOGF("topkey burned\n");
				}

				top_got_device_id = true;
				vTaskDelay(200);
			}else{
				LOGI("device id fail from top\n");
			}
		}
		break;

		case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
		{
			// Get morpheus device id request from Nordic
			LOGI("GET DEVICE ID\n");
			_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID);

			static bool played = false;
			if( !played && booted) {
				if(command->has_ble_bond_count)
				{
					LOGI("BOND COUNT %d\n", command->ble_bond_count);
					// this command fires before MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID
					// and it is the 1st command you can get from top.
					if(!command->ble_bond_count){
						// If we had ble_bond_count field, boot LED animation can start from here. Visual
						// delay of device boot can be greatly reduced.
						play_startup_sound();
						ble_proto_led_init();
					}else{
						ble_proto_led_init();
					}
				} else {
					LOGI("NO BOND COUNT\n");
					play_startup_sound();
					ble_proto_led_init();
				}
				played = true;
			}
		}
		break;
	}
	if(!booted || provisioning_mode) {
		return true;
	}
    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        {
        	set_ble_mode(BLE_CONNECTED);
            const char* ssid = command->wifiSSID.arg;
            char* password = command->wifiPassword.arg;

            // I can get the Mac address as well, but not sure it is necessary.


            int sec_type = SL_SEC_TYPE_WPA_WPA2;
            if(command->has_security_type)
            {
            	sec_type = command->security_type == wifi_endpoint_sec_type_SL_SCAN_SEC_TYPE_WPA2 ? SL_SEC_TYPE_WPA_WPA2 : command->security_type;
            }
            // Just call API to connect to WIFI.
#if 0
        	LOGI("Wifi SSID %s pswd ", ssid, password);
            if( sec_type == SL_SEC_TYPE_WEP ) {
            	int i;
            	for(i=0;i<strlen(password);++i) {
            		LOGI("%x:", password[i]);
            	}
        		LOGI("\n" );
            } else {
            	LOGI("%s\n", ssid, password);
            }
#endif
            int result = _set_wifi(ssid, (char*)password, sec_type, command->version );

        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
            // Light up LEDs?
			ble_proto_led_fade_in_trippy();
            set_ble_mode(BLE_PAIRING);
            LOGI( "PAIRING MODE \n");

			analytics_event( "{ble: pairing}" );
            //wifi prescan, forked so we don't block the BLE and it just happens in the background
			if(!scan_results){
				scan_results = prescan_wifi(MAX_WIFI_EP_PER_SCAN);
			}
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
		{
			//given on phone disconnect
			ble_proto_led_fade_out(0);
            set_ble_mode(BLE_NORMAL);
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
        	set_ble_mode(BLE_CONNECTED);
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
            int result = _pair_device(command, 0);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
        {
            LOGI("PAIR SENSE\n");
            int result = _pair_device(command, 1);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET:
        {
            LOGI("FACTORY RESET\n");
            _factory_reset();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN:
        {
            LOGI("WIFI Scan request\n");
            if(!scan_results){
            	scan_results = prescan_wifi(MAX_WIFI_EP_PER_SCAN);
            }
            if(scan_results){
            	_reply_wifi_scan_result();
            	hlo_future_destroy(scan_results);
            	scan_results = prescan_wifi(MAX_WIFI_EP_PER_SCAN);
            }else{
            	ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);

            }
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES:
        {
			#define MIN_SHAKE_INTERVAL 3000
            static portTickType last_shake = 0;
            portTickType now = xTaskGetTickCount();

            LOGI("PILL SHAKES\n");
            if( now - last_shake < MIN_SHAKE_INTERVAL ) {
                LOGI("PILL SHAKE THROTTLE\n");
            } else if(command->deviceId.arg){
				uint32_t color = pill_settings_get_color((const char*)command->deviceId.arg);
				uint8_t* argb = (uint8_t*)&color;

				if(color) {
					ble_proto_led_flash(0xFF, argb[1], argb[2], argb[3], 10);
				} else /*if(pill_settings_pill_count() == 0)*/ {
					ble_proto_led_flash(0xFF, 0x80, 0x00, 0x80, 10);
				}
				last_shake = xTaskGetTickCount();
            }else{
            	LOGI("Please update topboard, no pill id\n");
            }
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_BUSY:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_BUSY\n");
    		ble_proto_led_busy_mode(0xFF, 128, 0, 128, 18);
    		_ble_reply_command_with_type(command->type);
    		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_FAILED:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_FAILED\n");
    		ble_proto_led_fade_out(false);
    		_ble_reply_command_with_type(command->type);
    		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_SUCCESS:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_SUCCESS\n");
            ble_proto_led_fade_out(true);
            _ble_reply_command_with_type(command->type);
            break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_TRIPPY:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_TRIPPY\n");
    		ble_proto_led_fade_in_trippy();
    		_ble_reply_command_with_type(command->type);
    		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PUSH_DATA_AFTER_SET_TIMEZONE:
        {
            LOGI("Push data\n");
            if(force_data_push() != 0)
            {
                ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }else{
                _ble_reply_command_with_type(command->type);
            }
        }
        break;
        default:
        	LOGW("Deprecated BLE Command: %d\r\n", command->type);
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
        	break;
	}
    return true;
}
