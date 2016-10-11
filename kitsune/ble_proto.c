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
#include "audiotask.h"
#include "hlo_net_tools.h"

volatile static bool wifi_state_requested = false;
volatile static int bond_cnt = 0;

void delete_alarms();

typedef void(*task_routine_t)(void*);

typedef enum {
	LED_BUSY = 0,
	LED_TRIPPY,
	LED_OFF
}led_mode_t;

static volatile struct {
	uint8_t argb[4];
	int delay;
	volatile bool hold_released;
	volatile ble_mode_t ble_status;
    xSemaphoreHandle smphr;
} _self;

static SlWlanNetworkEntry_t _wifi_endpoints[MAX_WIFI_EP_PER_SCAN];
static xSemaphoreHandle _wifi_smphr;
static hlo_future_t * scan_results;
bool need_init_lights = false;
bool needs_startup_sound = false;
bool needs_pairing_animation = false;
ble_mode_t get_ble_mode() {
	ble_mode_t status;
	xSemaphoreTake( _self.smphr, portMAX_DELAY );
	status = _self.ble_status;
	xSemaphoreGive( _self.smphr );
	return status;
}
bool ble_user_active() {
	return get_ble_mode() == BLE_PAIRING || get_ble_mode() == BLE_CONNECTED;
}

static void set_ble_mode(ble_mode_t status) {
	xSemaphoreTake( _self.smphr, portMAX_DELAY );
	analytics_event( "{ble_mode: %d}", status );

	_self.ble_status = status;
	xSemaphoreGive( _self.smphr );
}
static bool get_released() {
	bool status;
	xSemaphoreTake( _self.smphr, portMAX_DELAY );
	status = _self.hold_released;
	xSemaphoreGive( _self.smphr );
	return status;
}
static void set_released(bool hold_released) {
	xSemaphoreTake( _self.smphr, portMAX_DELAY );
	_self.hold_released = hold_released;
	xSemaphoreGive( _self.smphr );
}
void ble_reply_http_status(char * status)
{
	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_CONNECTION_STATE;
	reply_command.has_wifi_connection_state = true;
	reply_command.wifi_connection_state = wifi_connection_state_CONNECTED;
	reply_command.has_http_response_code = true;
	memcpy( reply_command.http_response_code, status, 15);
	//need to only send when it has been requested....=
	if( wifi_state_requested ) {
        if(wifi_status_get(UPLOADING)) {
            wifi_state_requested = false;
        }
		ble_send_protobuf(&reply_command);
	}
}

void ble_reply_socket_error(int error)
{
	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_CONNECTION_STATE;
	reply_command.wifi_connection_state = wifi_connection_state_CONNECT_FAILED;
	reply_command.has_wifi_connection_state = true;
	reply_command.socket_error_code = error;
	reply_command.has_socket_error_code = true;
	//need to only send when it has been requested....=
	if( wifi_state_requested ) {
		ble_send_protobuf(&reply_command);
	}
}

void ble_reply_wifi_status(wifi_connection_state state)
{
	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_CONNECTION_STATE;
	reply_command.wifi_connection_state = state;
	reply_command.has_wifi_connection_state = true;
	//need to only send when it has been requested....
	//LOGI("blewifi status %d %d", state, wifi_state_requested );
	if( wifi_state_requested ) {
		ble_send_protobuf(&reply_command);
	}
}

static void _ble_reply_command_with_type(MorpheusCommand_CommandType type)
{
	MorpheusCommand reply_command;
	memset(&reply_command, 0, sizeof(reply_command));
	reply_command.type = type;
	ble_send_protobuf(&reply_command);
    LOGI("Sending BLE %d\n",type);
}
extern int deleteFilesInDir(const char* dir);
static void _factory_reset(){
	wifi_reset();
    reset_default_antenna();
    delete_alarms();
    pill_settings_reset_all();
    nwp_reset();
    deleteFilesInDir(USER_DIR);

	_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET);

	vTaskDelay(5000);
	mcu_reset(); //just in case top doesn't do it
}

int Cmd_factory_reset(int argc, char* argv[])
{
    _factory_reset();
	return 0;
}

#define PM_TIMEOUT (20*60*1000UL)
static TimerHandle_t pm_timer;

void pm_cancel( TimerHandle_t pxTimer ) {
	MorpheusCommand response = { 0 };
	if(get_ble_mode() == BLE_PAIRING && !needs_startup_sound && !needs_pairing_animation) {
		response.type =
				MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
		ble_send_protobuf(&response);
	}
}

void ble_proto_init() {
	vSemaphoreCreateBinary(_self.smphr);
	vSemaphoreCreateBinary(_wifi_smphr);
	set_ble_mode(BLE_NORMAL);

	pm_timer = xTimerCreate("PM Timer",PM_TIMEOUT,pdFALSE, 0, pm_cancel);
}


static void _reply_wifi_scan_result()
{
    int i = 0;
    MorpheusCommand reply_command = {0};
    int count = hlo_future_read_once(scan_results,_wifi_endpoints,sizeof(_wifi_endpoints) );

    for(i = 0; i < count; i++)
    {
		reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN;
		reply_command.wifi_scan_result.arg = &_wifi_endpoints[i];
		ble_send_protobuf(&reply_command);
        vTaskDelay(250);  // This number must be long enough so the BLE can get the data transmit to phone
        memset(&reply_command, 0, sizeof(reply_command));
    }
    if( count == -11 ) {
    	LOGI("WIFI SCAN TO\n");
    }
    reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_STOP_WIFISCAN;
	ble_send_protobuf(&reply_command);
	LOGI(">>>>>>Send WIFI scan results done<<<<<<\n");
	scan_results = prescan_wifi(MAX_WIFI_EP_PER_SCAN);
}
int force_data_push();
static bool _set_wifi(const char* ssid, const char* password, int security_type, int version, int app_version)
{
    int i,idx;
    idx = -1;

	LOGI("Connecting to WIFI %s\n", ssid );
	xSemaphoreTake(_wifi_smphr, portMAX_DELAY);
    for(i=0;i<MAX_WIFI_EP_PER_SCAN;++i) {
    	if( !strcmp( (char*)_wifi_endpoints[i].Ssid, ssid ) ) {
    		antsel(_wifi_endpoints[i].Reserved[0]);
    		save_default_antenna( _wifi_endpoints[i].Reserved[0] );
    		LOGI("RSSI %d\r\n", ssid, _wifi_endpoints[i].Rssi);
    		break;
    	}
    }
	xSemaphoreGive(_wifi_smphr);

	if(app_version >= 0) {
		int to = 0;

		nwp_reset();
		wifi_state_requested = true;

	    if(connect_wifi(ssid, password, security_type, version, &idx, false) < 0)
	    {
			LOGI("failed to connect\n");
	        ble_reply_protobuf_error(ErrorType_WLAN_CONNECTION_ERROR);
	        //led_set_color(0xFF, LED_MAX,0,0,1,1,20,0);
			return 0;
	    }
	    force_data_push();

		while( !wifi_status_get(UPLOADING) ) {
			vTaskDelay(9000);

			if( ++to > 3 ) {
				if( idx != -1 ) {
					sl_WlanProfileDel(idx);
					nwp_reset();
				}
				LOGI("wifi timeout\n");
				wifi_state_requested = false;
				ble_reply_protobuf_error(ErrorType_SERVER_CONNECTION_TIMEOUT);
				break;
			} else {
				if (wifi_status_get(CONNECTING)) {
					ble_reply_wifi_status(wifi_connection_state_WLAN_CONNECTING);
				} else if (wifi_status_get(CONNECT)) {
					ble_reply_wifi_status(wifi_connection_state_WLAN_CONNECTED);
				} else if (wifi_status_get(IP_LEASED)) {
					ble_reply_wifi_status(wifi_connection_state_IP_RETRIEVED);
				} else if (!wifi_status_get(0xFFFFFFFF)) {
					ble_reply_wifi_status(wifi_connection_state_NO_WLAN_CONNECTED);
				}
			}
		}
		if( wifi_status_get(UPLOADING) ) {
			#define INV_INDEX 0xff
			int _connected_index = INV_INDEX;
			int16_t ret = 0;
			uint8_t retry = 5;
			SlWlanSecParams_t secParam = make_sec_params(ssid, password, security_type, version);

			ret = sl_WlanProfileDel(0xFF);
			while(sl_WlanProfileAdd((_i8*) ssid, strlen(ssid), NULL,
					&secParam, NULL, 0, 0) < 0 && retry--){
				ret = sl_WlanProfileDel(0xFF);
				if (ret != 0) {
					LOGI("profile del fail\n");
				}
			}
			LOGI("index %d\n", _connected_index);
		} else {
			char ssid[MAX_SSID_LEN];
			char mac[6];
			SlWlanGetSecParamsExt_t extSec;
			uint32_t priority;
			int16_t namelen;
			SlWlanSecParams_t secParam;
			nwp_reset();
			if( 0 < sl_WlanProfileGet(0,(_i8*)ssid, &namelen, (_u8*)mac, &secParam, &extSec, (_u32*)&priority)) {
				sl_WlanConnect((_i8*) ssid, strlen(ssid), NULL, &secParam, 0);
			}
		}
		wifi_state_requested = false;
	} else {
		bool connection_ret = false;
		//play_led_progress_bar(0xFF, 128, 0, 128,portMAX_DELAY);
	    connect_wifi(ssid, password, security_type, version, &idx, false);
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
	}

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
extern xQueueHandle pill_prox_queue;

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
        switch(command->type){
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_PROX_DATA:
        	xQueueSend(pill_prox_queue, &command->pill_data, 10);
        	break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA:
        	 xQueueSend(pill_queue, &command->pill_data, 10);
        	break;
        }
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

typedef struct {
	MorpheusCommand cmd;
	int is_morpheus;
} pairing_context_t;

static void _on_pair_success(void * structdata){
	LOGF("pairing success\r\n");
	if( structdata ) {
		ble_send_protobuf(structdata);
	} else {
		ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
	}
}
static void _on_pair_failure(){
    LOGF("pairing server response failed\r\n");
	ble_reply_protobuf_error(ErrorType_NETWORK_ERROR);
}
static void _pair_reply(const NetworkResponse_t * response,
		char * reply_buf, int reply_sz, void * context) {
	pairing_context_t * ctx = (pairing_context_t*)context;
    ble_proto_free_command(&ctx->cmd);
    if( !ctx->is_morpheus && response->success ) {
    	force_data_push();
    }
    vPortFree(ctx);
}

void save_account_id( char * acct );
static bool _pair_device( MorpheusCommand* command, int is_morpheus)
{
	if(NULL == command->accountId.arg || NULL == command->deviceId.arg){
		LOGI("*******Missing fields\n");
		ble_reply_protobuf_error(ErrorType_INVALID_ACCOUNT_ID);
		return false;
	}

	save_account_id( command->accountId.arg );

	ble_proto_assign_encode_funcs(command);

	pairing_context_t * ctx = pvPortMalloc(sizeof(pairing_context_t));
	assert( ctx );
	ctx->is_morpheus = is_morpheus;
	memcpy( &ctx->cmd, command, sizeof(MorpheusCommand) ); //WARNING shallow copy

	protobuf_reply_callbacks pb_cb;
	pb_cb.get_reply_pb = ble_proto_get_morpheus_command;
	pb_cb.free_reply_pb = ble_proto_free_morpheus_command;
	pb_cb.on_pb_failure = _on_pair_failure;
	pb_cb.on_pb_success = _on_pair_success;

	NetworkTask_SendProtobuf( false,
				DATA_SERVER,
				is_morpheus == 1 ? MORPHEUS_REGISTER_ENDPOINT : PILL_REGISTER_ENDPOINT,
				MorpheusCommand_fields,
				&ctx->cmd,
				30000,
				_pair_reply, ctx, &pb_cb, true);

	return true; // success
}

void ble_proto_led_init()
{
	play_led_animation_solid(LED_MAX, LED_MAX, LED_MAX,LED_MAX,1, 33,1);
	led_is_idle(5000);
}


void ble_proto_led_busy_mode(uint8_t a, uint8_t r, uint8_t g, uint8_t b, int delay)
{
	LOGI("LED BUSY\n");
	_self.argb[0] = a;
	_self.argb[1] = r;
	_self.argb[2] = g;
	_self.argb[3] = b;
	_self.delay = delay;

	flush_animation_history();
	play_led_wheel(a,r,g,b,0,delay,2);
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

	play_led_animation_solid(a,r,g,b,2,18,1);
}
extern volatile bool provisioning_mode;

void ble_proto_led_fade_in_trippy(){
	uint8_t trippy_base[3] = { 25, 30, 230 };
	uint8_t trippy_range[3] = { 25, 30, 228 }; //last on wraps, but oh well

	flush_animation_history();
	play_led_trippy(trippy_base, trippy_range, portMAX_DELAY, 30, 30 );
}

void ble_proto_led_fade_out(bool operation_result){
	if(operation_result) {
		led_fade_all_animation(18);
		play_led_animation_solid(LED_MAX,LED_MAX,LED_MAX,LED_MAX,1,11,1);
	} else {
		led_fade_all_animation(18);
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

#define BLE_HOLD_TIMEOUT_MS 10000
void hold_animate_progress_task(void * params) {
	uint32_t start = xTaskGetTickCount();

	vTaskDelay(3000);
	if( get_released() ) {
		vTaskDelete(NULL);
		return;
	}
	LOGI("Trigger pairing mode\n");
	MorpheusCommand response = { 0 };
	response.type =
			MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE;
	ble_send_protobuf(&response);

	vTaskDelay(BLE_HOLD_TIMEOUT_MS);
	if( get_released() ) {
		vTaskDelete(NULL);
		return;
	}
	response.type =
			MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
	ble_send_protobuf(&response);

	vTaskDelete(NULL);
}

void ble_proto_start_hold()
{
	switch (get_ble_mode()) {
	case BLE_PAIRING: {
		if( bond_cnt > 0 ) {
			MorpheusCommand response = { 0 };
			// hold to cancel the pairing mode
			LOGI("pairing cancelled\n");
			response.type =
					MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
			ble_send_protobuf(&response);
		} else {
			LOGE("pairing cancelled but bond cnt %d", bond_cnt);
			Cmd_SyncID(0,0);
		}
		break;
	}
	case BLE_CONNECTED:
	default:
		set_released(false);
		xTaskCreate(hold_animate_progress_task, "hold_animate_pair",1024 / 4, NULL, 2, NULL);
	}

}

void ble_proto_end_hold()
{
	set_released(true);
}
#include "hellofilesystem.h"
#define STARTUP_SOUND_NAME "/ringtone/star003.raw"

void play_startup_sound() {
	// TODO: Play startup sound. You will only reach here once.
	// Now the hand hover-to-pairing mode will not delete all the bonds
	// when the bond db is full, so you will never get zero after a phone bonds
	// to Sense, unless user do factory reset and power cycle the device.

	if(needs_startup_sound){
		vTaskDelay(10);
		AudioPlaybackDesc_t desc;
		memset(&desc, 0, sizeof(desc));
		desc.stream = fs_stream_open(STARTUP_SOUND_NAME, HLO_STREAM_READ);
		ustrncpy(desc.source_name, STARTUP_SOUND_NAME, sizeof(desc.source_name));
		desc.volume = 32;
		desc.durationInSeconds = -1;
		desc.rate = AUDIO_SAMPLE_RATE;
		desc.fade_in_ms = 0;
		desc.fade_out_ms = 0;
		desc.to_fade_out_ms = 0;
		AudioTask_StartPlayback(&desc);
		needs_startup_sound = false;
		vTaskDelay(175);
	}
	if( need_init_lights ) {
		ble_proto_led_init();
		need_init_lights = false;
	}
	if(needs_pairing_animation){
		ble_proto_led_fade_in_trippy();
		needs_pairing_animation = false;
	}

}

#include "crypto.h"
extern volatile bool booted;
extern volatile bool provisioning_mode;
extern uint8_t aes_key[AES_BLOCKSIZE + 1];
int save_device_id( uint8_t * device_id );
int save_aes( uint8_t * key ) ;
uint8_t get_alpha_from_light();
bool is_test_boot();

char top_version[16] = {0};
const char * get_top_version(void){
	return top_version;
}
bool on_ble_protobuf_command(MorpheusCommand* command)
{
	bool finished_with_command = true;

	if(command->has_ble_bond_count) {
		static bool played = false;
		bond_cnt = command->ble_bond_count;

		if( !played && booted && !is_test_boot() && xTaskGetTickCount() < 5000 ) {
			if(command->has_ble_bond_count)
			{
				LOGI("BOND COUNT %d\n", command->ble_bond_count);
				// this command fires before MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID
				// and it is the 1st command you can get from top.
				if(!command->ble_bond_count){
					// If we had ble_bond_count field, boot LED animation can start from here. Visual
					// delay of device boot can be greatly reduced.
					needs_startup_sound = true;
				}
			} else {
				LOGI("NO BOND COUNT\n");
				needs_startup_sound = true;
			}
			need_init_lights = true;
			played = true;
		}
	}

	switch(command->type)
	{
		case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
		{
			LOGI("got SYNC_DEVICE_ID\n");
			int i;
			if(command->deviceId.arg){
				const char * device_id_str = command->deviceId.arg;
				for(i=0;i<DEVICE_ID_SZ;++i) {
					char num[3] = {0};
					memcpy( num, device_id_str+i*2, 2);
					top_device_id[i] = strtol( num, NULL, 16 );
				}
				LOGI("got id from top %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
						top_device_id[0],top_device_id[1],top_device_id[2],
						top_device_id[3],top_device_id[4],top_device_id[5],
						top_device_id[6],top_device_id[7]);
			    save_device_id(top_device_id);
				_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID);
				top_board_notify_boot_complete();

				set_ble_mode(BLE_NORMAL);

#if 0
				if(command->has_aes_key && should_burn_top_key()){
					save_aes(command->aes_key.bytes);
					LOGF("topkey burned\n");
				}
#endif

				top_got_device_id = true;

				if(command->has_top_version){
					strncpy(top_version,command->top_version, sizeof(top_version));
					LOGI("\r\nTop Board Version is %s\r\n", top_version);
				}else{
					strcpy(top_version, "unknown");
				}
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
		}
		break;
	}
	if(!booted || provisioning_mode) {
		goto cleanup;
	}
    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        {
            LOGI("set wifi %d %d %d\n", wifi_state_requested, command->has_app_version, command->app_version );

        	set_ble_mode(BLE_CONNECTED);
            const char* ssid = command->wifiSSID.arg;
            char* password = command->wifiPassword.arg;

            // I can get the Mac address as well, but not sure it is necessary.
            int sec_type = SL_WLAN_SEC_TYPE_WPA_WPA2;
            if(command->has_security_type)
            {
            	sec_type = command->security_type;
            }
            // Just call API to connect to WIFI.
#if 0
        	LOGI("Wifi SSID %s pswd ", ssid, password);
            if( sec_type == SL_WLAN_SEC_TYPE_WEP ) {
            	int i;
            	for(i=0;i<strlen(password);++i) {
            		LOGI("%x:", password[i]);
            	}
        		LOGI("\n" );
            } else {
            	LOGI("%s\n", ssid, password);
            }
#endif
            _set_wifi(ssid, (char*)password, sec_type, command->version,
            		command->has_app_version ? command->app_version : -1  );

        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:  // Just for testing
        {
    		if (get_ble_mode() != BLE_PAIRING) {
				// Light up LEDs?
    			needs_pairing_animation = true;
				set_ble_mode(BLE_PAIRING);
				LOGI( "PAIRING MODE \n");
#if 0
				//wifi prescan, forked so we don't block the BLE and it just happens in the background
				if(!scan_results){
					scan_results = prescan_wifi(MAX_WIFI_EP_PER_SCAN);
				}
#endif
				assert( pdPASS == xTimerStart(pm_timer, 30000));
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
        	ble_proto_led_fade_out(0);
        	set_ble_mode(BLE_CONNECTED);
        	LOGI("PHONE CONNECTED\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_BONDED:
        {
        	ble_proto_led_fade_out(0);
        	LOGI("PHONE BONDED\n");
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_PROX_DATA:
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
            finished_with_command = !_pair_device(command, 0);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
        {
            LOGI("PAIR SENSE\n");
            finished_with_command = !_pair_device(command, 1);
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

            if( command->has_country_code ) {
                uint16_t len = 4;
        	    uint16_t  config_opt = SL_WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE;
        	    char cc[4];
        	    sl_WlanGet(SL_WLAN_CFG_GENERAL_PARAM_ID, &config_opt, &len, (_u8* )cc);
        	    LOGI("Set country code %s have %s\n", command->country_code, cc );
        	    if( strncmp( cc, command->country_code, 2) != 0 ) {
					LOGI("mismatch\n");
					sl_enter_critical_region();
					sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
							SL_WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, 2,
							(uint8_t* )command->country_code);
					vTaskDelay(100);
					nwp_reset();
					sl_exit_critical_region();
        	    }
			}

            if(!scan_results){
            	scan_results = prescan_wifi(MAX_WIFI_EP_PER_SCAN);
            }
            if(scan_results){
            	_reply_wifi_scan_result();
            }else{
            	ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
            }
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES:
        {
			uint32_t color = pill_settings_get_color((const char*)command->deviceId.arg);
			uint8_t* argb = (uint8_t*)&color;
			if(color) {
				ble_proto_led_flash(get_alpha_from_light(), argb[1], argb[2], argb[3], 10);
			} else /*if(pill_settings_pill_count() == 0)*/ {
				ble_proto_led_flash(get_alpha_from_light(), 0x80, 0x00, 0x80, 10);
			}
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_BUSY:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_BUSY\n");
    		_ble_reply_command_with_type(command->type);
    		ble_proto_led_busy_mode(0xFF, 128, 0, 128, 18);
    		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_FAILED:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_FAILED\n");
    		_ble_reply_command_with_type(command->type);
    		ble_proto_led_fade_out(false);
    		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_SUCCESS:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_OPERATION_SUCCESS\n");
            _ble_reply_command_with_type(command->type);
            ble_proto_led_fade_out(true);
            break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_TRIPPY:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_LED_TRIPPY\n");
    		_ble_reply_command_with_type(command->type);
    		ble_proto_led_fade_in_trippy();
    		break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PUSH_DATA_AFTER_SET_TIMEZONE:
        {
            LOGI("Push data\n");
            if(force_data_push() != 0)
            {
                ble_reply_protobuf_error(ErrorType_FORCE_DATA_PUSH_FAILED);
            }else{
                _ble_reply_command_with_type(command->type);
            }
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_COUNTRY_CODE:
        	if( command->has_country_code ) {
				sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
						SL_WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, 2, (uint8_t*)command->country_code);
				nwp_reset();
        	} else {
                ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
        	}

        	break;
        default:
        	LOGW("Deprecated BLE Command: %d\r\n", command->type);
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
        	break;
	}
    cleanup:
    if( finished_with_command ) {
    	ble_proto_free_command(command);
    }
    return true;
}
int Cmd_SyncID(int argc, char * argv[]){
	_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID);
	return 0;
}
