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
    uint8_t trippy_base[3];
    uint8_t trippy_range[3];
} _self;

static int _wifi_read_index;
static int _scanned_wifi_count = 0;
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

    reset_default_antenna();
    pill_settings_reset_all();
    nwp_reset();
    deleteFilesInDir(USER_DIR);
	_ble_reply_command_with_type(MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET);

}

static int scan_with_retry( Sl_WlanNetworkEntry_t ** endpoints, int antenna ) {
	int scan_cnt = 0;

	*endpoints = pvPortMalloc( MAX_WIFI_EP_PER_SCAN * sizeof(Sl_WlanNetworkEntry_t) );
	assert(*endpoints);
	memset(*endpoints, 0, MAX_WIFI_EP_PER_SCAN * sizeof(Sl_WlanNetworkEntry_t) );

	uint8_t max_retry = 3;
	uint8_t retry_count = max_retry;

	while((scan_cnt = get_wifi_scan_result(*endpoints, MAX_WIFI_EP_PER_SCAN, 3000 * (max_retry - retry_count + 1), antenna)) == 0 && --retry_count)
	{
		LOGI("No wifi scanned, retry times remain %d\n", retry_count);
		vTaskDelay(500);
	}

	return scan_cnt;
}
static void debug_print_ssid( char * msg, Sl_WlanNetworkEntry_t * ep, int n ) {
	int i,b;
    LOGI( msg );
	for(i=0;i<n;++i) {
		LOGI( "%s %d %d %d", ep[i].ssid, ep[i].rssi, ep[i].reserved[0] );
		for(b=0;b<SL_BSSID_LENGTH;++b) {
			LOGI("%x:", ep[i].bssid[b] );
		}
		LOGI("\n");
		vTaskDelay(10);
	}
}

static int compare_rssi (const void * a, const void * b)
{
  return ((Sl_WlanNetworkEntry_t*)b)->rssi - ((Sl_WlanNetworkEntry_t*)a)->rssi;
}

static void sort_on_ssid( Sl_WlanNetworkEntry_t * ep, int n ) {
	qsort( ep, n, sizeof(Sl_WlanNetworkEntry_t), compare_rssi );
}

static void dedupe_ssid( Sl_WlanNetworkEntry_t * ep, int * c){
	int j,i;
	for (i = 0; i < *c - 1; ++i) {
		//LOGI( "OUTER %d %d %d\n", i, j, *c);
		for (j = i + 1; j < *c; ++j) {
			vTaskDelay(10);
			//LOGI( "INNER %d %d %d\n", i, j, *c);
			if(!strcmp((char*)ep[i].ssid, (char*)ep[j].ssid)) {
				LOGI( "MATCH %s %s\n", ep[i].ssid, ep[j].ssid);
				vTaskDelay(10);
				memcpy( ep+j, ep+j+1, *c - j - 1 );
				--*c;
				i = -1;

				//debug_print_ssid( "UPDATED ", ep, *c );
				break;
			}
		}
	}
}

static int _scan_wifi()
{
	Sl_WlanNetworkEntry_t * endpoints_ifa;
	Sl_WlanNetworkEntry_t * endpoints_pcb;
	int scan_cnt[ANTENNA_CNT+1] = {0};

	//only scan if we've been reset, this lets us cache the scan result for later
	if( _scanned_wifi_count != 0 ) {
		return _scanned_wifi_count;
	}
	memset(_wifi_endpoints, 0, sizeof(_wifi_endpoints));
	_scanned_wifi_count = 0;
	_wifi_read_index = 0;

	wifi_status_set(SCANNING, false);  // Set the scanning flag

	scan_cnt[IFA_ANT] = scan_with_retry( &endpoints_ifa, IFA_ANT );
	scan_cnt[PCB_ANT] = scan_with_retry( &endpoints_pcb, PCB_ANT );

	int i,p;

	debug_print_ssid( "SSID RSSI IFA\n", endpoints_ifa, scan_cnt[IFA_ANT] );
	debug_print_ssid( "SSID RSSI PCB\n", endpoints_pcb, scan_cnt[PCB_ANT] );
	sort_on_ssid( endpoints_ifa, scan_cnt[IFA_ANT] );
	sort_on_ssid( endpoints_pcb, scan_cnt[PCB_ANT] );
	debug_print_ssid( "SSID RSSI IFA SORTED\n", endpoints_ifa, scan_cnt[IFA_ANT] );
	debug_print_ssid( "SSID RSSI PCB SORTED\n", endpoints_pcb, scan_cnt[PCB_ANT] );
	dedupe_ssid(  endpoints_ifa, &scan_cnt[IFA_ANT]);
	LOGI("DEDUPE BARRIER\n");
	dedupe_ssid(  endpoints_pcb, &scan_cnt[PCB_ANT]);
	debug_print_ssid( "SSID RSSI IFA UNIQUE\n", endpoints_ifa, scan_cnt[IFA_ANT] );
	debug_print_ssid( "SSID RSSI PCB UNIQUE\n", endpoints_pcb, scan_cnt[PCB_ANT] );

	LOGI("BEGIN MERGE\n");
	//merge the lists... since they are sorted by rssi we can pop the best one
	//however the two lists can contain repeated values... so we need to scan out the dupes with lesser signal, better to just do it ahead of time
	for(i=0;i<scan_cnt[IFA_ANT];++i) {
		for(p=0;p<scan_cnt[PCB_ANT];++p) {
			if(!strcmp((char*)endpoints_pcb[p].ssid, (char*)endpoints_ifa[i].ssid)) {
				LOGI("MATCH\n");
				if( endpoints_ifa[i].rssi > endpoints_pcb[p].rssi ) {
					LOGI("%s %d\n", endpoints_ifa[i].ssid, endpoints_ifa[i].rssi);
		            LOGI("%s %d\n", endpoints_pcb[p].ssid, endpoints_pcb[p].rssi);
					LOGI("REMOVE PCB %d %d %d %d\n", i,p,scan_cnt[IFA_ANT],scan_cnt[PCB_ANT]);
					memmove( &endpoints_pcb[p], &endpoints_pcb[p+1], sizeof( Sl_WlanNetworkEntry_t ) * (MAX_WIFI_EP_PER_SCAN - p - 1) );
					--scan_cnt[PCB_ANT];
					--p;
				} else {
					LOGI("%s %d\n", endpoints_ifa[i].ssid, endpoints_ifa[i].rssi);
		            LOGI("%s %d\n", endpoints_pcb[p].ssid, endpoints_pcb[p].rssi);
					LOGI("REMOVE IFA %d %d %d %d\n", i,p,scan_cnt[IFA_ANT],scan_cnt[PCB_ANT]);
					memmove( &endpoints_ifa[i], &endpoints_ifa[i+1], sizeof( Sl_WlanNetworkEntry_t ) * (MAX_WIFI_EP_PER_SCAN - i - 1) );
					--scan_cnt[IFA_ANT];
					--i;
				}
			}
		}
	}

	debug_print_ssid( "SSID RSSI IFA MERGED\n", endpoints_ifa, scan_cnt[IFA_ANT] );
	debug_print_ssid( "SSID RSSI PCB MERGED\n", endpoints_pcb, scan_cnt[PCB_ANT] );

	for(_scanned_wifi_count = 0;_scanned_wifi_count<MAX_WIFI_EP_PER_SCAN;) {
		if( scan_cnt[IFA_ANT] && endpoints_ifa[0].rssi > endpoints_pcb[0].rssi ) {
			memcpy( &_wifi_endpoints[_scanned_wifi_count], &endpoints_ifa[0], sizeof( Sl_WlanNetworkEntry_t ) );
			memmove( &endpoints_ifa[0], &endpoints_ifa[0+1], sizeof( Sl_WlanNetworkEntry_t ) * (MAX_WIFI_EP_PER_SCAN - 0 - 1) );
			_wifi_endpoints[_scanned_wifi_count].reserved[0] = IFA_ANT;
			--scan_cnt[IFA_ANT];
			LOGI("PICKED IFA %d %d\n", _scanned_wifi_count, scan_cnt[IFA_ANT]);
		} else if( scan_cnt[PCB_ANT] && endpoints_pcb[0].rssi > endpoints_ifa[0].rssi ) {
			memcpy( &_wifi_endpoints[_scanned_wifi_count], &endpoints_pcb[0], sizeof( Sl_WlanNetworkEntry_t ) );
			_wifi_endpoints[_scanned_wifi_count].reserved[0] = PCB_ANT;
			memmove( &endpoints_pcb[0], &endpoints_pcb[0+1], sizeof( Sl_WlanNetworkEntry_t ) * (MAX_WIFI_EP_PER_SCAN - 0 - 1) );
			--scan_cnt[PCB_ANT];
			LOGI("PICKED PCB %d %d\n", _scanned_wifi_count, scan_cnt[PCB_ANT]);
		} else if( scan_cnt[IFA_ANT] ){
			memcpy(&_wifi_endpoints[_scanned_wifi_count], &endpoints_ifa[0], sizeof(Sl_WlanNetworkEntry_t));
			memmove( &endpoints_ifa[0], &endpoints_ifa[0+1], sizeof( Sl_WlanNetworkEntry_t ) * (MAX_WIFI_EP_PER_SCAN - 0 - 1) );
			_wifi_endpoints[_scanned_wifi_count].reserved[0] = IFA_ANT;
			--scan_cnt[IFA_ANT];
			LOGI("PICKED IFA %d %d\n", _scanned_wifi_count, scan_cnt[IFA_ANT]);
		} else if( scan_cnt[PCB_ANT] ){
			memcpy(&_wifi_endpoints[_scanned_wifi_count], &endpoints_pcb[0], sizeof(Sl_WlanNetworkEntry_t));
			memmove( &endpoints_pcb[0], &endpoints_pcb[0+1], sizeof( Sl_WlanNetworkEntry_t ) * (MAX_WIFI_EP_PER_SCAN - 0 - 1) );
			_wifi_endpoints[_scanned_wifi_count].reserved[0] = PCB_ANT;
			--scan_cnt[PCB_ANT];
			LOGI("PICKED PCB %d %d\n", _scanned_wifi_count, scan_cnt[PCB_ANT]);
		} else {
			LOGI("PICKED NONE %d %d %d\n", _scanned_wifi_count, scan_cnt[IFA_ANT], scan_cnt[PCB_ANT]);
			break;
		}

		++_scanned_wifi_count;
		debug_print_ssid( "SSID RSSI COMBINED STEP \n", _wifi_endpoints, _scanned_wifi_count );

	}
	debug_print_ssid( "SSID RSSI COMBINED\n", _wifi_endpoints, _scanned_wifi_count );
	dedupe_ssid(  _wifi_endpoints, &_scanned_wifi_count );
	debug_print_ssid( "SSID RSSI UNIQUE\n", _wifi_endpoints, _scanned_wifi_count );

	wifi_status_set(SCANNING, true);  // Remove the sanning flag
	vPortFree(endpoints_pcb);
	vPortFree(endpoints_ifa);
	return _scanned_wifi_count;
}

static void _reply_next_wifi_ap()
{
	//reset so the next scan command will do a scan
	if(_wifi_read_index == _scanned_wifi_count || _wifi_read_index == MAX_WIFI_EP_PER_SCAN) {
		_wifi_read_index = 0;
		_scanned_wifi_count = 0;
	}

	MorpheusCommand reply_command = {0};
	reply_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_NEXT_WIFI_AP;
	reply_command.wifi_scan_result.arg = &_wifi_endpoints[_wifi_read_index++];
	ble_send_protobuf(&reply_command);
}

static void _reply_wifi_scan_result()
{
    int i = 0;
    MorpheusCommand reply_command = {0};

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


static bool _set_wifi(const char* ssid, const char* password, int security_type)
{
    int connection_ret, i;

    uint8_t max_retry = 10;
    uint8_t retry_count = max_retry;

    for(i=0;i<MAX_WIFI_EP_PER_SCAN;++i) {
    	if( !strcmp( (char*)_wifi_endpoints[i].ssid, ssid ) ) {
    		antsel(_wifi_endpoints[i].reserved[0]);
    		save_default_antenna( _wifi_endpoints[i].reserved[0] );
    		break;
    	}
    }

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

		// Wait until the disconnect event happen. If we have a WIFI connection already
		// and the user enter the wrong password in the next WIFI setup, it will go straight to success
		// without returning error.
		vTaskDelay(1000);
		wifi_status_set(CONNECTING, false);


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
int _force_data_push()
{
    if(!wait_for_time(10))
    {
    	//ble_reply_protobuf_error(ErrorType_NETWORK_ERROR);
    	LOGE("Cannot get time\n");
		return -1;
    }

    periodic_data* data = pvPortMalloc(sizeof(periodic_data));  // Let's put this in the heap, we don't use it all the time
	if(!data)
	{
		//ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
		LOGE("No memory\n");
		return -2;
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
	int ret = -1;
	if(NULL == command->accountId.arg || NULL == command->deviceId.arg){
		LOGI("*******Missing fields\n");
		ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
	}else{

		ble_proto_assign_encode_funcs(command);
		// TODO: Figure out why always get -1 when this is the 1st request
		// after the IPv4 retrieved.

		int retry = 3;
		while(ret != 0 && retry--)
		{
			ret = NetworkTask_SynchronousSendProtobuf(
					DATA_SERVER,
					is_morpheus == 1 ? MORPHEUS_REGISTER_ENDPOINT : PILL_REGISTER_ENDPOINT,
					response_buffer,
					sizeof(response_buffer),
					MorpheusCommand_fields,
					command,
					0);
			vTaskDelay(1000);
		}

		// All the args are in stack, don't need to do protobuf free.

		if(ret == 0)
		{
			_send_response_to_ble(response_buffer, sizeof(response_buffer));
			return 1;
		}else{
			LOGI("Pairing request failed, error %d\n", ret);
			ble_reply_protobuf_error(ErrorType_NETWORK_ERROR);
		}
		if(!is_morpheus) {
			vTaskDelay(1000);
			_force_data_push();
		}
	}

	return 0; // failure
}

static int _sync_led_basic_animation(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b,
        int fade_in, int fade_out,
        unsigned int ud,
        int rot)
{
    led_set_color(alpha, r, g, b, fade_in, fade_out, ud, rot);

    // After set color, we dont care what is the next state of LED and we shouldn't care
    // just go ahead and block the calling thread.
    int fade_delay = led_delay(ud);

    int total_delay = 0;
    if(fade_in)
    {
        total_delay += fade_delay;
    }

    if(fade_out)
    {
        total_delay += fade_delay;
    }

    vTaskDelay(total_delay);
    return led_ready();  // may or may not ready, depends on the deplay we compute, but here we dont care.
}

void _sync_stop_trippy_animation(){
    stop_led_animation();
    memset(_self.trippy_base, 0, sizeof(_self.trippy_base));
    memset(_self.trippy_range, 0, sizeof(_self.trippy_range));
    vTaskDelay(led_delay(TRIPPY_FADE_DELAY));
}

void ble_proto_led_init()
{
	_self.led_status = LED_OFF;
	led_set_color(0xFF, LED_MAX, LED_MAX, LED_MAX, 1, 1, 36, 0);
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
    _sync_led_basic_animation(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 1, 0, _self.delay, 1);
}

void ble_proto_led_flash(int a, int r, int g, int b, int delay, int time)
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
	int i;
	if(_self.led_status == LED_TRIPPY)
	{
		ble_proto_led_fade_out(0);
		_self.led_status = LED_BUSY;
		for(i = 0; i < time; i++)
		{
			_sync_led_basic_animation(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 1, 1, _self.delay, 0);
		}

		_self.led_status = LED_OFF;
		ble_proto_led_fade_in_trippy();
	}else if(_self.led_status == LED_OFF){
		_self.led_status = LED_BUSY;
		for(i = 0; i < time; i++)
		{
			_sync_led_basic_animation(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 1, 1, _self.delay, 0);
		}
		_self.led_status = LED_OFF;
	}

}

void ble_proto_led_fade_in_custom_trippy(uint8_t base[3], uint8_t range[3]){
	switch(_self.led_status)
	{
	case LED_BUSY:
		_sync_led_basic_animation(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 0, 1, 18, 0);
		play_led_trippy(base, range, portMAX_DELAY);

		break;
	case LED_TRIPPY:
		if(memcmp(_self.trippy_base, base, 3) != 0 || memcmp(_self.trippy_range, range, 3) != 0)
		{
			ble_proto_led_fade_out(false);
			LOGI("redo trippy\n");
			play_led_trippy(base, range, portMAX_DELAY);
		}
		break;
	case LED_OFF:
		play_led_trippy(base, range, portMAX_DELAY);
		break;
	}

	_self.led_status = LED_TRIPPY;
}

void ble_proto_led_fade_in_trippy(){
	uint8_t trippy_base[3] = {60, 25, 90};
	ble_proto_led_fade_in_custom_trippy(trippy_base, trippy_base);
}

void ble_proto_led_fade_out(bool operation_result){
	switch(_self.led_status)
	{
	case LED_BUSY:
		if(operation_result)
		{
			_sync_led_basic_animation(0xFF, LED_MAX, LED_MAX, LED_MAX, 0, 1, 18, 0);
		}else{
			_sync_led_basic_animation(_self.argb[0], _self.argb[1], _self.argb[2], _self.argb[3], 0, 1, 18, 1);
		}
		break;
	case LED_TRIPPY:
		_sync_stop_trippy_animation();
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
		if (_self.ble_status != BLE_PAIRING) {
			LOGI("Trigger pairing mode\n");
			MorpheusCommand response = { 0 };
			response.type =
					MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE;
			ble_send_protobuf(&response);
			ble_proto_led_fade_in_trippy();
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
		AudioTask_StartPlayback(&desc);
	}
	vTaskDelay(175);
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
            //we are pairing so it's likely we are beginning the onboarding. This will prescan the wifi.
            _scan_wifi();
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
            LOGI( "GET_WIFI\n");
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

            static bool played = false;
            if( !played ) {
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
            _scan_wifi();
            _reply_wifi_scan_result();
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES:
        {
            LOGI("PILL SHAKES\n");
            if(command->deviceId.arg){
				uint32_t color = pill_settings_get_color((const char*)command->deviceId.arg);
				uint8_t* argb = (uint8_t*)&color;

				if(color) {
					ble_proto_led_flash(0xFF, argb[1], argb[2], argb[3], 10, 2);
				} else if(_self.led_status == LED_TRIPPY || pill_settings_pill_count() == 0) {
					ble_proto_led_flash(0xFF, 0x80, 0x00, 0x80, 10, 2);
				}
            }else{
            	LOGI("Please update topboard, no pill id\n");
            }
        }
        break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
        {
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID\n");
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
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SCAN_WIFI:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_SCAN_WIFI\n");
    		_scan_wifi();
    		_ble_reply_command_with_type(command->type);
    		break;
    	case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_NEXT_WIFI_AP:
    		LOGI("MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_NEXT_WIFI_AP\n");
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
