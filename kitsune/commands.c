//*****************************************************************************
//
// commands.c - FreeRTOS porting example on CCS4
//
//*****************************************************************************
#include <stdio.h>
#include <string.h>
#include "kit_assert.h"

#include <hw_types.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <pin.h>
#include <uart.h>
#include <stdarg.h>
#include <stdlib.h>
#include "utils.h"
#include "interrupt.h"

#include <string.h>

#include "sdhost.h"
#include "gpio.h"
#include "rom_map.h"

#include "simplelink.h"
#include "wlan.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

#include "fault.h"

#include "cmdline.h"
#include "ustdlib.h"
#include "uartstdio.h"

#include "networktask.h"
#include "wifi_cmd.h"
#include "i2c_if.h"
#include "i2c_cmd.h"
#include "dust_cmd.h"
#include "fatfs_cmd.h"
#include "spi_cmd.h"
#include "audioprocessingtask.h"
#include "audiotask.h"
#include "top_board.h"
#include "fft.h"

/* I2S module*/
#include "i2s.h"
#include "mcasp_if.h"
#include "udma.h"
#include "udma_if.h"
#include "pcm_handler.h"
#include "circ_buff.h"
#include "pcm_handler.h"
#include "osi.h"

#include "control.h"
#include "network.h"

#include "diskio.h"
#include "top_hci.h"
#include "slip_packet.h"
#include "ble_proto.h"
#include "led_cmd.h"
#include "led_animations.h"
#include "uart_logger.h"

#include "kitsune_version.h"
#include "TestNetwork.h"
#include "sys_time.h"
#include "gesture.h"
#include "fs.h"
#include "sl_sync_include_after_simplelink_header.h"

#ifdef KIT_INCLUDE_FILE_UPLOAD
#include "fileuploadertask.h"
#endif
#include "hellofilesystem.h"

#include "hw_ver.h"
#include "pinmux.h"

#include "proto_utils.h"
#include "ustdlib.h"

#include "hlo_stream.h"
#include "pill_settings.h"
#include "prox_signal.h"
#include "hlo_audio_tools.h"
#include "hlo_audio.h"
#include "hlo_net_tools.h"
#include "top_board.h"
#include "long_poll.h"
#include "filedownloadmanager.h"


#include "audiohelper.h"

#define ONLY_MID 0

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))


#include "hlo_proto_tools.h"

//******************************************************************************
//			        FUNCTION DECLARATIONS
//******************************************************************************

//******************************************************************************
//			    GLOBAL VARIABLES
//******************************************************************************

tCircularBuffer *pTxBuffer = NULL;
tCircularBuffer *pRxBuffer = NULL;

volatile bool booted = false;
volatile bool use_dev_server = false;

//*****************************************************************************
//                          LOCAL DEFINES
//*****************************************************************************

//// ==============================================================================
//// The CPU usage in percent, in 16.16 fixed point format.
//// ==============================================================================
//extern unsigned long g_ulCPUUsage;
//
//// ==============================================================================
//// This function implements the "cpu" command.  It prints the CPU type, speed
//// and current percentage utilization.
//// ==============================================================================
//int Cmd_cpu(int argc, char *argv[])
//{
//    //
//    // Print some header text.
//    //
//    LOGI("ARM Cortex-M4F %u MHz - ",configCPU_CLOCK_HZ / 1000000);
//    LOGI("%2u%% utilization\n", (g_ulCPUUsage+32768) >> 16);
//
//    // Return success.
//    return(0);
//}

static unsigned int heap_high_mark = 0;
static unsigned int heap_low_mark = 0xffffffff;
static unsigned int heap_print=0;

void usertraceMALLOC( void * pvAddress, size_t uiSize ) {
	if( xPortGetFreeHeapSize() > heap_high_mark ) {
		heap_high_mark = xPortGetFreeHeapSize();
	}
	if (xPortGetFreeHeapSize() < heap_low_mark) {
		heap_low_mark = xPortGetFreeHeapSize();
	}
	if( heap_print ) {
		UARTprintf( "%d +%d\n",xPortGetFreeHeapSize(), uiSize );
	}
}

void usertraceFREE( void * pvAddress, size_t uiSize ) {
	if (xPortGetFreeHeapSize() > heap_high_mark) {
		heap_high_mark = xPortGetFreeHeapSize();
	}
	if (xPortGetFreeHeapSize() < heap_low_mark) {
		heap_low_mark = xPortGetFreeHeapSize();
	}
	if( heap_print ) {
		UARTprintf( "%d -%d\n",xPortGetFreeHeapSize(), uiSize );
	}
}
// ==============================================================================
// This function implements the "free" command.  It prints the free memory.
// ==============================================================================
int Cmd_free(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	LOGF("heap %d +: %d -: %d\n", xPortGetFreeHeapSize(),heap_high_mark,heap_low_mark);

    heap_high_mark = 0;
	heap_low_mark = 0xffffffff;
	if( argc != 0 ) {
		heap_print = atoi(argv[1]);
	}
	// Return success.
	return (0);
}

int Cmd_fs_write(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	unsigned long tok=0;
	long hndl, bytes;
	SlFsFileInfo_t info;

#ifndef DEBUG_FS
	if (strstr(argv[1], "cert") != 0) {
		LOGE("unauthorized\n");
		return 0;
	}
	if (strstr(argv[1], "hello") != 0) {
		LOGE("unauthorized\n");
		return 0;
	}
	if (strstr(argv[1], "sys") != 0) {
		LOGE("unauthorized\n");
		return 0;
	}
	if (strstr(argv[1], "top") != 0) {
		LOGE("unauthorized\n");
		return 0;
	}
#endif

	sl_FsGetInfo((unsigned char*)argv[1], tok, &info);

	if (hndl = sl_FsOpen((unsigned char*)argv[1],
			SL_FS_WRITE, &tok) < 0 ) {
		LOGF("error opening file, trying to create\n");

		if ( hndl = sl_FsOpen((unsigned char*)argv[1],
				SL_FS_CREATE_NOSIGNATURE | SL_FS_CREATE_MAX_SIZE( 65535 ),
				&tok) < 0 ) {
			LOGF("error opening for write\n");
			return -1;
		}
	}
    char* next = argv[2];
    int cnt = 0;
    int expected = argc - 2;
	while( cnt != expected ) {
        uint8_t byte = strtol(next, &next, 16);
    	bytes = sl_FsWrite(hndl, info.Len + cnt, (unsigned char*)&byte, 1);
    	assert(bytes==1);
    	++cnt;
    	next = next + 1;
    }

	LOGF("wrote to the file %d bytes\n", cnt);

	sl_FsClose(hndl, 0, 0, 0);

	// Return success.
	return (0);
}
int Cmd_fs_read(int argc, char *argv[]) {
	//
	// Print some header text.
	//
#define minval( a,b ) a < b ? a : b
#define BUF_SZ 600
	unsigned long tok=0;
	long hndl, err, bytes, i;
	SlFsFileInfo_t info;
	char buffer[BUF_SZ];

#ifndef DEBUG_FS
	if (strstr(argv[1], "cert") != 0) {
		LOGF("unauthorized\n");
		return 0;
	}
	if (strstr(argv[1], "hello") != 0) {
		LOGF("unauthorized\n");
		return 0;
	}
	if (strstr(argv[1], "sys") != 0) {
		LOGF("unauthorized\n");
		return 0;
	}
	if (strstr(argv[1], "top") != 0) {
		LOGF("unauthorized\n");
		return 0;
	}
#endif

	sl_FsGetInfo((unsigned char*)argv[1], tok, &info);

	hndl = sl_FsOpen((unsigned char*) argv[1], SL_FS_READ, &tok );
	if (hndl < 0 ) {
		LOGF("error opening for read %d\n", hndl);
		return -1;
	}
	if( argc >= 3 ){
		bytes = sl_FsRead(hndl, atoi(argv[2]), (unsigned char* ) buffer,
				minval(info.Len, BUF_SZ));
		if (bytes) {
			LOGF("read %d bytes\n", bytes);
		}
	} else {
		bytes = sl_FsRead(hndl, 0, (unsigned char* ) buffer,
				minval(info.Len, BUF_SZ));
		if (bytes) {
			LOGF("read %d bytes\n", bytes);
		}
	}


	sl_FsClose(hndl, 0, 0, 0);

	for (i = 0; i < bytes; ++i) {
		LOGF("%x", buffer[i]);
	}

	return 0;
}

int Cmd_audio_turn_on(int argc, char * argv[]) {
	AudioTask_StartCapture(AUDIO_CAPTURE_PLAYBACK_RATE);

	AudioProcessingTask_SetControl(featureUploadsOn,0);
#ifdef KIT_INCLUDE_FILE_UPLOAD
	AudioProcessingTask_SetControl(rawUploadsOn,0);
#endif

	return 0;
}

int Cmd_stop_buff(int argc, char *argv[]) {
	AudioTask_StopPlayback();

	return 0;
}

static
OctogramResult_t octorgram_result;
static
xSemaphoreHandle octogram_semaphore = 0;

int Cmd_get_octogram(int argc, char * argv[]) {
	xSemaphoreTake(octogram_semaphore,portMAX_DELAY);
	xSemaphoreGive(octogram_semaphore);

	int avg = 0;
	avg += octorgram_result.logenergy[3];
	avg += octorgram_result.logenergy[4];
	avg += octorgram_result.logenergy[5];
	avg += octorgram_result.logenergy[6];
	avg /= 4;

	LOGF("%d\r\n", octorgram_result.logenergy[2] - avg );

	return 0;
}


int Cmd_fs_delete(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	int err = sl_FsDel((unsigned char*)argv[1], 0);
	if (err) {
		LOGF("error %d\n", err);
		return -1;
	}

	// Return success.
	return (0);
}

#include "fs_utils.h"
#define PROV_CODE "provision"
volatile bool provisioning_mode = false;
#include "crypto.h"

void check_provision() {
	char buf[64] = {0};
	int read = 0;
	provisioning_mode = false;

	fs_get( PROVISION_FILE, buf, sizeof(buf), &read);
	if (read == strlen(PROV_CODE)) {
		if (0 == strncmp(buf, PROV_CODE, read)) {
			provisioning_mode = true;
			LOGI("provisioning mode!\n");
		}
	}
}
static char serial[64];

void load_serial() {
	memset(serial, 0, sizeof(serial));
	fs_get(SERIAL_FILE, serial, sizeof(serial), NULL);
}


static xSemaphoreHandle alarm_smphr;
static SyncResponse_Alarm alarm;
static char alarm_ack[sizeof(((SyncResponse *)0)->ring_time_ack)];
static volatile bool needs_alarm_ack = false;
#define ALARM_LOC "/hello/alarm"
static bool alarm_is_ringing = false;

void delete_alarms() {
	sl_FsDel((unsigned char*)ALARM_LOC, 0);
}

void set_alarm( SyncResponse_Alarm * received_alarm, const char * ack, size_t ack_size ) {
    if (xSemaphoreTakeRecursive(alarm_smphr, portMAX_DELAY)) {
    	if( alarm_is_ringing ) {
    		LOGI("already ringing\n");
            xSemaphoreGiveRecursive(alarm_smphr);
            return;
    	}
		if ( ack && ack_size && memcmp(alarm_ack, ack, ack_size) == 0) {
			LOGI("alarm already set\n");
		} else if (received_alarm->has_ring_offset_from_now_in_second
        	&& received_alarm->ring_offset_from_now_in_second > -1 ) {   // -1 means user has no alarm/reset his/her now
        	unsigned long now = get_time();
        	received_alarm->start_time = now + received_alarm->ring_offset_from_now_in_second;

        	int ring_duration = received_alarm->has_ring_duration_in_second ? received_alarm->ring_duration_in_second : 30;
        	received_alarm->end_time = now + received_alarm->ring_offset_from_now_in_second + ring_duration;
        	received_alarm->has_end_time = received_alarm->has_start_time = received_alarm->has_ring_duration_in_second = true;
        	received_alarm->ring_duration_in_second = ring_duration;
        	// //sanity check

        	// since received_alarm->ring_offset_from_now_in_second >= 0, we don't need to check received_alarm->start_time
            
            memcpy(&alarm, received_alarm, sizeof(alarm));
            LOGI("alarm %d to %d in %d minutes\n",
                        received_alarm->start_time, received_alarm->end_time,
                        (received_alarm->start_time - now) / 60);
            if(ack && ack_size){
				memcpy(alarm_ack, ack, min(sizeof(alarm_ack), ack_size));
				ack_size = ack_size >= sizeof(alarm_ack) ? sizeof(alarm_ack)-1 : ack_size;
				alarm_ack[ack_size] = 0;
				LOGI("Alarm ID: %s\r\n", alarm_ack );
				fs_save( ALARM_LOC, received_alarm, sizeof(alarm));
				needs_alarm_ack = true;
			}else{
				memset(alarm_ack, 0, sizeof(alarm_ack));
			}
        }else{
            LOGI("No alarm\n");
            // when we reach here, we need to cancel the existing alarm to prevent them ringing.

            // The following is not necessary, putting here just to make them explicit.
            received_alarm->start_time = 0;
            received_alarm->end_time = 0;
            received_alarm->has_start_time = false;
            received_alarm->has_end_time = false;
            received_alarm->has_ring_offset_from_now_in_second = false;

            memcpy(&alarm, received_alarm, sizeof(alarm));
        }
        xSemaphoreGiveRecursive(alarm_smphr);
    }
}

int load_alarm( ) {
	return fs_get( ALARM_LOC, &alarm, sizeof(alarm), NULL );
}

static bool cancel_alarm() {
	bool was_ringing = false;
	if(xTaskGetTickCount() > 10000) {
		AudioTask_StopPlayback();
	}

	if (xSemaphoreTakeRecursive(alarm_smphr, portMAX_DELAY)) {
		if (alarm_is_ringing) {
			if (alarm.has_end_time || alarm.has_ring_offset_from_now_in_second) {
				analytics_event( "{alarm: dismissed}" );
				LOGI("ALARM DONE RINGING\n");
				alarm.has_end_time = 0;
				alarm.has_start_time = false;
				alarm.has_ring_offset_from_now_in_second = false;
			}
		    alarm_is_ringing = false;
		    was_ringing = true;
		}

		xSemaphoreGiveRecursive(alarm_smphr);
	}
	return was_ringing;
}

int set_test_alarm(int argc, char *argv[]) {
	SyncResponse_Alarm alarm;
	unsigned int now = get_time();
	alarm.end_time = now + 45;
	alarm.start_time = now + 5;
	alarm.ring_duration_in_second = 40;
	alarm.ring_offset_from_now_in_second = 5;
	strncpy(alarm.ringtone_path, "/ringtone/tone.raw",
			strlen("/ringtone/tone.raw"));

	alarm.has_end_time = 1;
	alarm.has_start_time = 1;
	alarm.has_ring_duration_in_second = 1;
	alarm.has_ringtone_id = 0;
	alarm.has_ringtone_path = 1;
	alarm.has_ring_offset_from_now_in_second = 1;

	char ack[32];
	usnprintf(ack, 32, "%d", now);
	set_alarm(&alarm, ack, strlen(ack));

	return 0;
}
static void thread_alarm_on_finished(void * context) {
	if( led_get_animation_id() == *(int*)context ) {
		stop_led_animation(10, 60);
	}
	if (xSemaphoreTakeRecursive(alarm_smphr, 500)) {
		LOGI("Alarm finished\r\n");
		alarm_is_ringing = false;
		xSemaphoreGiveRecursive(alarm_smphr);

	}
}

static bool _is_file_exists(char* path)
{
	FILINFO finfo;
	FRESULT res = hello_fs_stat(path, &finfo);

	if (res != FR_OK) {
		return false;
	}
	return true;
}
#include "hellofilesystem.h"
uint8_t get_alpha_from_light();
void thread_alarm(void * unused) {
	int alarm_led_id = -1;
	while (1) {
		wait_for_time(WAIT_FOREVER);

		portTickType now = xTaskGetTickCount();
		uint32_t time = get_time();
		// The alarm thread should go ahead even without a valid time,
		// because we don't need a correct time to fire alarm, we just need the offset.
		if (xSemaphoreTakeRecursive(alarm_smphr, portMAX_DELAY)) {
			if( time - alarm.start_time < 600 || alarm.start_time - time < 600 ) {
				if( time < alarm.start_time ) {
					LOGI("coming %d in %d\n",
								alarm.start_time,
								(alarm.start_time-time));
				} else {
					LOGI("past %d in %d\n",
								alarm.start_time,
								(time - alarm.start_time));
				}
			}
			if ( time - alarm.start_time < 600 ) {
				AudioPlaybackDesc_t desc;
				memset(&desc,0,sizeof(desc));

				desc.to_fade_out_ms = desc.fade_in_ms = 30000;
				desc.fade_out_ms = 3000;
				int has_valid_sound_file = 0;
				char file_name[64] = {0};
				if(alarm.has_ringtone_path)
				{
					memcpy(file_name, alarm.ringtone_path, sizeof(alarm.ringtone_path) <= 64 ? sizeof(alarm.ringtone_path) : 64);
					file_name[63] = 0;
					if(_is_file_exists(file_name))
					{
						has_valid_sound_file = 1;
					}

				}

				if(!has_valid_sound_file)
				{
					memset(file_name, 0, sizeof(file_name));
					// fallback for DVT
					char* fallback = "/RINGTONE/DIGO001.raw";
					char* fallback_short = "/RINGTO~1/DIGO001.raw";
					if(_is_file_exists(fallback))
					{
						memcpy(file_name, fallback, strlen(fallback));
						has_valid_sound_file = 1;
					}else if(_is_file_exists(fallback_short)){
						memcpy(file_name, fallback_short, strlen(fallback_short));
						has_valid_sound_file = 1;
					}
				}

				if(!has_valid_sound_file)
				{
					memset(file_name, 0, sizeof(file_name));
					// fallback for PVT
					char* fallback = "/RINGTONE/DIG001.raw";
					if(_is_file_exists(fallback))
					{
						memcpy(file_name, fallback, strlen(fallback));
						has_valid_sound_file = 1;
					}
				}

				if(!has_valid_sound_file)
				{
					LOGE("ALARM RING FAIL: NO RINGTONE FILE FOUND %s\n", file_name);
				}

				desc.stream = fs_stream_open(file_name,HLO_STREAM_READ);
				ustrncpy(desc.source_name, file_name, sizeof(desc.source_name));
				desc.durationInSeconds = alarm.ring_duration_in_second;
				desc.volume = 10;
				desc.onFinished = thread_alarm_on_finished;
				desc.rate = AUDIO_CAPTURE_PLAYBACK_RATE;
				desc.context = &alarm_led_id;

				alarm.has_start_time = FALSE;
				alarm.start_time = 0;
				AudioTask_StartPlayback(&desc);

				LOGI("ALARM RINGING RING RING RING\n");
				LOGI("ALARM DURATION %d\n", alarm.ring_duration_in_second);
				analytics_event( "{alarm: ring}" );
				alarm_is_ringing = true;

				uint8_t trippy_base[3] = { 0, 0, 0 };
				uint8_t trippy_range[3] = { 254, 254, 254 };
				alarm_led_id = play_led_trippy(trippy_base, trippy_range,0,30, 120000);
			}
			
			xSemaphoreGiveRecursive(alarm_smphr);
		}
		vTaskDelayUntil(&now, 1000);
	}
}

#define SENSOR_RATE 60

static int dust_m2,dust_mean,dust_log_sum,dust_max,dust_min,dust_cnt;
xSemaphoreHandle dust_smphr;
void init_dust();
unsigned int median_filter(unsigned int x, unsigned int * buf,unsigned int * p) {
	buf[(*p)++%3] = x;
	if( *p < 3 ) {
		return x;
	}
	if (buf[0] > buf[1]) {
		if (buf[1] > buf[2]) {
			return buf[1];
		}
	} else if (buf[2] < buf[0]) {
		return buf[0];
	}
	return buf[2];
}
static void thread_dust(void * unused) {
#define maxval( a, b ) a>b ? a : b
	dust_min = 5000;
	dust_m2 = dust_mean = dust_cnt = dust_log_sum = dust_max = 0;
#if DEBUG_DUST
	int dust_variability=0;
#endif
	unsigned int filter_buf[3];
	unsigned int filter_idx=0;
	while (1) {
		uint32_t now = xTaskGetTickCount();
		if (xSemaphoreTake(dust_smphr, portMAX_DELAY)) {
			unsigned int dust = get_dust();

			if (dust != DUST_SENSOR_NOT_READY) {
				dust = median_filter(dust, filter_buf, &filter_idx);

				dust_log_sum += bitlog(dust);
				++dust_cnt;

				int delta = dust - dust_mean;
				dust_mean = dust_mean + delta/dust_cnt;
				dust_m2 = dust_m2 + delta * ( dust - dust_mean);
				if( dust_m2 < 0 ) {
					dust_m2 = 0x7FFFFFFF;
				}
				if(dust > dust_max) {
					dust_max = dust;
				}
				if(dust < dust_min) {
					dust_min = dust;
				}

#if DEBUG_DUST
				if(dust_cnt > 1)
				{
					dust_variability = dust_m2 / (dust_cnt - 1);  // devide by zero again, add if
				}
				LOGF("%u,%u,%u,%u,%u,%u\n", xTaskGetTickCount(), dust, dust_mean, dust_max, dust_min, dust_variability );
#endif

			}
			xSemaphoreGive(dust_smphr);
		}
		vTaskDelayUntil(&now, 2500+(rand()%1000));
	}
}


static int light_m2,light_mean, light_cnt,light_log_sum,light_sf,light, rgb[3];
static xSemaphoreHandle light_smphr;
xSemaphoreHandle i2c_smphr;

uint8_t get_alpha_from_light()
{
	int adjust_max_light = 800;
	int adjust;


	xSemaphoreTake(light_smphr, portMAX_DELAY);
	if( light > adjust_max_light ) {
		adjust = adjust_max_light;
	} else {
		adjust = light_mean;
	}
	xSemaphoreGive(light_smphr);

	uint8_t alpha = 0xFF * adjust / adjust_max_light;
	alpha = alpha < 10 ? 10 : alpha;
	return alpha;
}


static int _is_light_off()
{
	static int last_light = -1;
	static unsigned int last_light_time = 0;
	const int light_off_threshold = 500;
	int ret = 0;

	xSemaphoreTake(light_smphr, portMAX_DELAY);
	if(last_light != -1)
	{
		int delta = last_light - light;
		if(xTaskGetTickCount() - last_light_time > 2000
				&& delta >= light_off_threshold
				&& light < 1000)
		{
			LOGI("light delta: %d, current %d, last %d\n", delta, light, last_light);
			ret = 1;
			light_mean = light; //so the led alpha will be at the lights off level
			last_light_time = xTaskGetTickCount();
		}
	}

	last_light = light;
	xSemaphoreGive(light_smphr);
	return ret;

}

#include "gesture.h"

static void _show_led_status()
{
	uint8_t alpha = get_alpha_from_light();
	bool light = alpha > 128;

	if(wifi_status_get(UPLOADING)) {
		unsigned int rgb[3] = { LED_MAX };
		led_get_user_color(&rgb[0], &rgb[1], &rgb[2], light);
		//led_set_color(alpha, rgb[0], rgb[1], rgb[2], 1, 1, 18, 0);
		play_led_animation_solid(alpha, rgb[0], rgb[1], rgb[2],1, 18,3);
	}
	else if(wifi_status_get(HAS_IP)) {
		play_led_wheel(alpha, LED_MAX,0,0,2,18,3);
	}
	else if(wifi_status_get(CONNECTING)) {
		play_led_wheel(alpha, LED_MAX,LED_MAX,0,2,18,3);
	}
	else if(wifi_status_get(SCANNING)) {
		play_led_wheel(alpha, LED_MAX,0,0,1,18,3);
	} else {
		play_led_wheel(alpha, LED_MAX,LED_MAX,LED_MAX,2,18,3);
	}
}

static void _on_wave(){
	if(	cancel_alarm() ) {
		stop_led_animation( 0, 33 );
	} else {
		_show_led_status();
	}
}

static void _on_hold(){
	stop_led_animation( 0, 33 );
	cancel_alarm();
	ble_proto_start_hold();
}

static void _on_gesture_out()
{
	ble_proto_end_hold();
}

int Cmd_gesture(int argc, char * argv[]) {
	if( strcmp(argv[1], "h") == 0){
		_on_hold();
	}
	if( strcmp(argv[1], "w") == 0){
		_on_wave();
	}
	if( strcmp(argv[1], "o") == 0){
		_on_gesture_out();
	}
	return 0;
}

void thread_fast_i2c_poll(void * unused)  {
	unsigned int filter_buf[3];
	unsigned int filter_idx=0;
	int w,r,g,b,p;

	gesture_init();
	ProxSignal_Init();
	ProxGesture_t gesture;

	uint32_t delay = 100;

	while (1) {
		portTickType now = xTaskGetTickCount();
		uint32_t prox=0;

		if (xSemaphoreTakeRecursive(i2c_smphr, 300000)) {
			vTaskDelay(1);

			if( 0 != get_rgb_prox( &w,&r,&g,&b,&p ) ) {
				goto fail_fast_i2c;
			}
			LOGP("%d,%d,%d,%d,%d\n", w,r,g,b,p );

			prox = median_filter(p, filter_buf, &filter_idx);

			gesture = ProxSignal_UpdateChangeSignals(prox);

			//gesture gesture_state = gesture_input(prox);
			switch(gesture)
			{
			case proxGestureWave:
				_on_wave();
				gesture_increment_wave_count();
				break;
			case proxGestureHold:
				_on_hold();
				gesture_increment_hold_count();
				break;
			case proxGestureRelease:
				_on_gesture_out();
				break;
			default:
				break;
			}

			if (xSemaphoreTake(light_smphr, portMAX_DELAY)) {
				light = w;
				rgb[0] = r;
				rgb[1] = g;
				rgb[2] = b;
				light_log_sum += bitlog(light);
				++light_cnt;

				int delta = light - light_mean;
				light_mean = light_mean + delta/light_cnt;
				light_m2 = light_m2 + delta * (light - light_mean);
				if( light_m2 < 0 ) {
					light_m2 = 0x7FFFFFFF;
				}
//				LOGI( "%d\t%d\t%d\t%d\t%d\n", delta, light_mean, light_m2, light_cnt, _is_light_off());
				xSemaphoreGive(light_smphr);

				if(light_cnt % 5 == 0 && led_is_idle(0) ) {
					if(_is_light_off()) {
						_show_led_status();
					}
				}
			}

			vTaskDelay(1);
			xSemaphoreGiveRecursive(i2c_smphr);
		} else {
			fail_fast_i2c:
			LOGW("failed to get i2c %d\n", __LINE__);
		}
		vTaskDelayUntil(&now, delay);
	}
}

#define MAX_PERIODIC_DATA 1
#define MAX_PILL_DATA 1
#define MAX_BATCH_PILL_DATA 1
#define PILL_BATCH_WATERMARK 0
#define MAX_BATCH_SIZE 1
#define ONE_HOUR_IN_MS ( 3600 * 1000 )

xQueueHandle data_queue = 0;
xQueueHandle force_data_queue = 0;
xQueueHandle pill_queue = 0;
xQueueHandle pill_prox_queue = 0;

extern volatile bool top_got_device_id;
extern volatile portTickType last_upload_time;
int send_top(char * s, int n) ;
void load_device_id();
bool is_test_boot();
//no need for semaphore, only thread_tx uses this one
int data_queue_batch_size = 1;
int pill_queue_batch_size = PILL_BATCH_WATERMARK;
typedef enum{
	MOTION = 0,
	PROX
}pill_batch_type;

static void handle_pill_queue(xQueueHandle queue, const char * endpoint, pill_batch_type type){
	batched_pill_data pill_data_batched = {0};
	if (uxQueueMessagesWaiting(queue) > pill_queue_batch_size) {
		pilldata_to_encode pilldata;
		pilldata.num_pills = 0;
		pilldata.pills = (pill_data*)pvPortMalloc(MAX_BATCH_PILL_DATA*sizeof(pill_data));

		if( !pilldata.pills ) {
			LOGE( "failed to alloc pilldata\n" );
			vTaskDelay(1000);
			return;
		}

		while( pilldata.num_pills < MAX_BATCH_PILL_DATA && xQueueReceive(queue, &pilldata.pills[pilldata.num_pills], 1 ) ) {
			++pilldata.num_pills;
		}

		memset(&pill_data_batched, 0, sizeof(pill_data_batched));
		pill_data_batched.device_id.funcs.encode = encode_device_id_string;
		switch(type){
			case MOTION:
				pill_data_batched.pills.funcs.encode = encode_all_pills;
				pill_data_batched.pills.arg = &pilldata;
				break;
			case PROX:
				DISP("Encoding PROX\r\n");
				pill_data_batched.prox.funcs.encode = encode_all_prox;
				pill_data_batched.prox.arg = &pilldata;
				break;
			default:
				goto end;
		}
		if(endpoint){
			send_pill_data_generic(&pill_data_batched, endpoint);
		}
end:
		vPortFree( pilldata.pills );
	}
}
#include "endpoints.h"
#include "hlo_http.h"
void thread_tx(void* unused) {
	batched_periodic_data data_batched = {0};
#ifdef UPLOAD_AP_INFO
	batched_periodic_data_wifi_access_point ap;
#endif
	periodic_data forced_data;
	bool got_forced_data = false;

	LOGI(" Start polling  \n");
	while (1) {
		if (uxQueueMessagesWaiting(data_queue) >= data_queue_batch_size
		 || got_forced_data ) {
			LOGI(	"sending data\n" );

			periodic_data_to_encode periodicdata;
			periodicdata.num_data = 0;
			periodicdata.data = (periodic_data*)pvPortMalloc(MAX_BATCH_SIZE*sizeof(periodic_data));

			if( !periodicdata.data ) {
				LOGI( "failed to alloc periodicdata\n" );
				vTaskDelay(1000);
				continue;
			}
			if( got_forced_data ) {
				memcpy( &periodicdata.data[periodicdata.num_data], &forced_data, sizeof(forced_data) );
				++periodicdata.num_data;
			}
			while( periodicdata.num_data < MAX_BATCH_SIZE && xQueueReceive(data_queue, &periodicdata.data[periodicdata.num_data], 1 ) ) {
				++periodicdata.num_data;
			}

			pack_batched_periodic_data(&data_batched, &periodicdata);

			data_batched.has_uptime_in_second = true;
			data_batched.uptime_in_second = xTaskGetTickCount() / configTICK_RATE_HZ;

			if( !is_test_boot() && provisioning_mode ) {
				//wait for top to boot...
#if 0
				top_got_device_id = false;
#endif
				if( !top_got_device_id ) {
					send_top( "rst", strlen("rst"));
				}
				while( !top_got_device_id ) {
					vTaskDelay(1000);
				}
#if 0
				save_aes_in_memory(DEFAULT_KEY);
#endif

#ifdef SELF_PROVISION_SUPPORT
				//try a test key with whatever we have so long as it is not the default
				if( !has_default_key() ) {
					uint8_t current_key[AES_BLOCKSIZE] = {0};
					get_aes(current_key);
					on_key(current_key);
				} else {
					ProvisionRequest pr;
					memset(&pr, 0, sizeof(pr));
					pr.device_id.funcs.encode = encode_device_id_string;
					pr.serial.funcs.encode = _encode_string_fields;
					pr.serial.arg = serial;
					pr.need_key = true;
					send_provision_request(&pr);
				}
#endif
			}

			wifi_get_connected_ssid( (uint8_t*)data_batched.connected_ssid, sizeof(data_batched) );
			data_batched.has_connected_ssid = true;
#ifdef UPLOAD_AP_INFO
			data_batched.scan.funcs.encode = encode_single_ssid;
			data_batched.scan.arg = &ap;
#endif
			if (xSemaphoreTakeRecursive(alarm_smphr, 1000)) {
				if(needs_alarm_ack){
					data_batched.has_ring_time_ack = true;
					memcpy(data_batched.ring_time_ack, alarm_ack, sizeof(data_batched.ring_time_ack));
					LOGI("Alarm ID: %s\r\n", alarm_ack );
				}
				xSemaphoreGiveRecursive(alarm_smphr);
			}
			data_batched.has_messages_in_queue = true;
			data_batched.messages_in_queue = uxQueueMessagesWaiting(data_queue);

			if( send_periodic_data(&data_batched, got_forced_data, ( MAX_PERIODIC_DATA - uxQueueMessagesWaiting(data_queue) ) * 60 * 1000 ) ) {
				last_upload_time = xTaskGetTickCount();
			}

			vPortFree( periodicdata.data );
			got_forced_data = false;
		}

		handle_pill_queue(pill_queue, PILL_DATA_RECEIVE_ENDPOINT, (pill_batch_type)MOTION);
		handle_pill_queue(pill_prox_queue, PILL_DATA_RECEIVE_ENDPOINT,  (pill_batch_type)PROX);

		do {
			if( xQueueReceive(force_data_queue, &forced_data, 1000 ) ) {
				got_forced_data = true;
			}
		} while (!wifi_status_get(HAS_IP));
	}
}
#include "audio_types.h"

void sample_sensor_data(periodic_data* data)
{
	if(!data )
	{
		return;
	}

	AudioOncePerMinuteData_t aud_data;
	data->unix_time = get_time();
	data->has_unix_time = true;

	// copy over the dust values
	if( xSemaphoreTake(dust_smphr, portMAX_DELAY)) {
		if( dust_cnt != 0 ) {
			data->dust = dust_mean;
			data->has_dust = true;

			dust_log_sum /= dust_cnt;  // devide by zero?
			if(dust_cnt > 1)
			{
				data->dust_variability = dust_m2 / (dust_cnt - 1);  // devide by zero again, add if
				data->has_dust_variability = true;  // since init with 0, by default it is false
			}
			data->has_dust_max = true;
			data->dust_max = dust_max;

			data->has_dust_min = true;
			data->dust_min = dust_min;
		} else {
			data->dust = (int)get_dust();
			if(data->dust == DUST_SENSOR_NOT_READY)  // This means we get some error?
			{
				data->has_dust = false;
			}
			data->has_dust_variability = false;
			data->has_dust_max = false;
			data->has_dust_min = false;
		}
		
		dust_min = 5000;
		dust_m2 = dust_mean = dust_cnt = dust_log_sum = dust_max = 0;
		xSemaphoreGive(dust_smphr);
	} else {
		data->has_dust = false;  // if Semaphore take failed, don't upload
	}

	//get audio -- this is thread safe
	AudioTask_DumpOncePerMinuteStats(&aud_data);

	if (aud_data.isValid) {
		data->has_audio_num_disturbances = true;
		data->audio_num_disturbances = aud_data.num_disturbances;

		data->has_audio_peak_background_energy_db = true;
		data->audio_peak_background_energy_db = aud_data.peak_background_energy;

		data->has_audio_peak_energy_db = true;
		data->audio_peak_energy_db = aud_data.peak_energy;

		data->has_audio_peak_disturbance_energy_db = true;
		if( aud_data.num_disturbances ) {
			data->audio_peak_disturbance_energy_db = aud_data.peak_energy;
		} else {
			data->audio_peak_disturbance_energy_db = aud_data.peak_background_energy;
		}
		//LOGI("Uploading audio %u %u %u\r\n",data->audio_num_disturbances, data->audio_peak_background_energy_db, data->audio_peak_disturbance_energy_db );
	}

	// copy over light values
	if (xSemaphoreTake(light_smphr, portMAX_DELAY)) {
		if(light_cnt == 0)
		{
			data->has_light = false;
		}else{
			light_log_sum /= light_cnt;  // just be careful for devide by zero.
			light_sf = (light_mean << 8) / bitexp( light_log_sum );

			if(light_cnt > 1)
			{
				data->light_variability = light_m2 / (light_cnt - 1);
				data->has_light_variability = true;
			}else{
				data->has_light_variability = false;
			}

			//LOGI( "%d lightsf %d var %d cnt\n", light_sf, light_var, light_cnt );
			data->light_tonality = light_sf;
			data->has_light_tonality = true;

			data->light = light_mean;
			data->has_light = true;

			light_m2 = light_mean = light_cnt = light_log_sum = light_sf = 0;
		}
		data->has_rgb = true;
		data->rgb.r = rgb[0];
		data->rgb.g = rgb[1];
		data->rgb.b = rgb[2];
		
		xSemaphoreGive(light_smphr);
	}

	{
		int ir;
		if( 0 == get_ir( &ir ) ) {
			LOGI("ir %d\n", ir);
			data->infrared = true;
			data->infrared = ir;
		}
	}
	{
	// get temperature and humidity
	uint32_t humid,press;
	int32_t temp;

	if( 0 == get_temp_press_hum(&temp, &press, &humid) ) {
		data->humidity = humid;
		data->temperature = temp;
		data->pressure = press;
		data->has_pressure = true;
		data->has_temperature = true;
		data->has_humidity = true;
		{
			int tvoc, eco2, current, voltage;
			if( 0 == get_tvoc( &tvoc, &eco2, &current, &voltage, temp, humid )) {
				LOGI("\nTVOC %d,%d,%d,%d,%d,%d,%d\n", data->unix_time, tvoc, eco2, current, voltage, temp, humid );
				data->has_tvoc = true;
				data->tvoc = tvoc;
				data->has_co2 = true;
				data->co2 = eco2;
			}
		}
		}
	}

	int wave_count = gesture_get_wave_count();
	if(wave_count > 0)
	{
		data->has_wave_count = true;
		data->wave_count = wave_count;
	}

	int hold_count = gesture_get_hold_count();
	if(hold_count > 0)
	{
		data->has_hold_count = true;
		data->hold_count = hold_count;
	}

	gesture_counter_reset();
}

int force_data_push()
{
    if(!wait_for_time(10))
    {
    	LOGE("Cannot get time\n");
		return -1;
    }

    periodic_data data;
    memset(&data, 0, sizeof(periodic_data));
    sample_sensor_data(&data);
    xQueueSend(force_data_queue, (void* )&data, 0);

    return 0;
}
static void print_nwp_version();
int Cmd_tasks(int argc, char *argv[]);
int Cmd_inttemp(int argc, char *argv[]);
void thread_sensor_poll(void* unused) {
	periodic_data data = {0};
	unsigned int count = 0;

	while (1) {
		portTickType now = xTaskGetTickCount();

		memset(&data, 0, sizeof(data));  // Don't forget re-init!

		wait_for_time(WAIT_FOREVER);

		sample_sensor_data(&data);

		if( booted ) {
			LOGI(	"collecting time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d %d %d %d\twave %d\thold %d\n",
					data.unix_time, data.light, data.light_variability,
					data.light_tonality, data.temperature, data.humidity,
					data.dust, data.dust_max, data.dust_min,
					data.dust_variability, data.wave_count, data.hold_count);

			Cmd_free(0,0);
			send_top("free", strlen("free"));
			//Cmd_inttemp(0,0);

			if( ( ++count % 60 ) == 0 ) {
				Cmd_tasks(0,0);
				LOGI("uptime %d, %d\n", xTaskGetTickCount(), count);
				print_nwp_version();
			}

			if (!xQueueSend(data_queue, (void* )&data, 0) == pdPASS) {
				xQueueReceive(data_queue, (void* )&data, 0); //discard one, so if the queue is full we will put every other one in the queue
				LOGE("Failed to post data\n");
			}
		}

		vTaskDelayUntil(&now, 60 * configTICK_RATE_HZ);
	}



}

// ==============================================================================
// This function implements the "tasks" command.  It prints a list of the
// current FreeRTOS tasks with information about status, stack usage, etc.
// ==============================================================================
#if ( configUSE_TRACE_FACILITY == 1 )

int Cmd_tasks(int argc, char *argv[]) {
	char* pBuffer;
	int i, l;

	LOGF("\t\t\t\t\tUnused\n            TaskName\tStatus\tPri\tStack\tTask ID\n");
	pBuffer = pvPortMalloc(2048);
	assert(pBuffer);
	LOGF("==========================");
	vTaskList(pBuffer);
	LOGF("==========================\n");
	l = strlen(pBuffer);
	for( i=0;i<l;++i ) {
		LOGF("%c", pBuffer[i]);
		vTaskDelay(1);
	}

	vPortFree(pBuffer);
	return 0;
}

#endif /* configUSE_TRACE_FACILITY */

#define SCAN_TABLE_SIZE   20

static void SortByRSSI(SlWlanNetworkEntry_t* netEntries,
                                            unsigned char ucSSIDCount)
{
	SlWlanNetworkEntry_t tTempNetEntry;
    unsigned char ucCount, ucSwapped;
    do{
        ucSwapped = 0;
        for(ucCount =0; ucCount < ucSSIDCount - 1; ucCount++)
        {
           if(netEntries[ucCount].Rssi < netEntries[ucCount + 1].Rssi)
           {
              tTempNetEntry = netEntries[ucCount];
              netEntries[ucCount] = netEntries[ucCount + 1];
              netEntries[ucCount + 1] = tTempNetEntry;
              ucSwapped = 1;
           }
        } //end for
     }while(ucSwapped);
}


int Cmd_rssi(int argc, char *argv[]) {
	int lCountSSID,i;
	int antenna = 0; // 0 does not change the antenna
	int duration = 1000;

	SlWlanNetworkEntry_t g_netEntries[SCAN_TABLE_SIZE];

	if (argc == 2) {
		duration = atoi(argv[1]);
	}
	if (argc == 3) {
		antenna = atoi(argv[2]);
	}

	lCountSSID = get_wifi_scan_result(&g_netEntries[0], SCAN_TABLE_SIZE, duration, antenna );

    SortByRSSI(&g_netEntries[0],(unsigned char)lCountSSID);

    LOGF( "SSID RSSI\n" );
	for(i=0;i<lCountSSID;++i) {
		LOGF( "%s %d\n", g_netEntries[i].Ssid, g_netEntries[i].Rssi );
	}
	return 0;
}

#include "crypto.h"
static const uint8_t exponent[] = { 1,0,1 };
static const uint8_t public_key[] = {
		00,0xcc,0x94,0xc1,0xc0,0x68,0x1f,0xd2,0x19,0xa8,0xc7,0xcd,0x82,0x3f,0x4e,
		    0x3a,0x22,0x37,0x8b,0xf3,0xa4,0x88,0xac,0xd5,0x40,0x94,0x76,0xc9,0x13,0xe5,
		    0xef,0x20,0xc9,0xf0,0xa4,0xed,0xf8,0xda,0xc6,0x9e,0x79,0x7b,0x57,0x8b,0xfe,
		    0x24,0x2d,0x45,0x37,0x33,0x21,0x45,0x0f,0xe4,0x94,0x82,0xc3,0xe9,0xfa,0xb7,
		    0x3d,0x90,0x6f,0xfc,0x90,0x05,0x45,0x90,0xc6,0x11,0x9c,0xad,0x2a,0x6b,0xbe,
		    0xf2,0xd8,0x85,0xc7,0x12,0xb5,0xb1,0x62,0xea,0x87,0xa3,0x6b,0xfb,0x9f,0xba,
		    0x4b,0xdc,0xe5,0x24,0x1d,0x02,0x83,0x82,0x17,0xd2,0x19,0xfe,0x94,0xd3,0x48,
		    0xe1,0xb2,0x34,0x90,0x23,0x55,0xc0,0x3d,0xbb,0x09,0xa7,0x6c,0x26,0x40,0xfb,
		    0x54,0xe4,0x88,0xb1,0xb1,0xb4,0xde,0xe5,0xb3
};
uint8_t top_device_id[DEVICE_ID_SZ];
volatile bool top_got_device_id = false; //being bad, this is only for factory

int save_aes( uint8_t * key ) ;
int save_device_id( uint8_t * device_id );
int Cmd_generate_factory_data(int argc,char * argv[]) {
#define NORDIC_ENC_ROOT_SIZE 16 //todo find out the real value
	uint8_t factory_data[AES_BLOCKSIZE + DEVICE_ID_SZ + SHA1_SIZE + 3];
	uint8_t enc_factory_data[255];
	char key_string[2*255];
	SHA1_CTX sha_ctx;
	RSA_CTX * rsa_ptr = NULL;
	int enc_size;
	uint8_t entropy_pool[32];

	if( !top_got_device_id ) {
		LOGE("Error please connect TOP board!\n");
		return -1;
	}

	//ENTROPY ! Sensors, timers, TI's mac address, so much randomness!!!11!!1!
	int pos=0;
	unsigned char mac[6];
	unsigned short mac_len;
	sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET, NULL, &mac_len, mac);
	memcpy(entropy_pool+pos, mac, 6);
	pos+=6;
	uint32_t now = xTaskGetTickCount();
	memcpy(entropy_pool+pos, &now, 4);
	pos+=4;
	for(; pos < 32; ++pos){
		unsigned int dust = get_dust();
		entropy_pool[pos] ^= (uint8_t)dust;
	}
	RNG_custom_init(entropy_pool, pos);

	//generate a key...
	get_random_NZ(AES_BLOCKSIZE, factory_data);
	factory_data[AES_BLOCKSIZE] = 0;
	save_aes(factory_data); //todo DVT enable

    //todo DVT get top's device ID, print it here, and use it as device ID in periodic/audio data
    save_device_id(top_device_id);
    memcpy( factory_data+AES_BLOCKSIZE + 1, top_device_id, DEVICE_ID_SZ);
	factory_data[AES_BLOCKSIZE+DEVICE_ID_SZ+1] = 0;

	//add checksum
	SHA1_Init( &sha_ctx );
	SHA1_Update( &sha_ctx, factory_data, AES_BLOCKSIZE+DEVICE_ID_SZ + 2  );
	SHA1_Final( factory_data+AES_BLOCKSIZE+DEVICE_ID_SZ + 2, &sha_ctx );
	factory_data[AES_BLOCKSIZE+DEVICE_ID_SZ+SHA1_SIZE+2] = 0;

	//init the rsa public key, encrypt the aes key and checksum
	RSA_pub_key_new( &rsa_ptr, public_key, sizeof(public_key), exponent, sizeof(exponent) );
	enc_size = RSA_encrypt(  rsa_ptr, factory_data, AES_BLOCKSIZE+DEVICE_ID_SZ+SHA1_SIZE+3, enc_factory_data, 0);
	RSA_free( rsa_ptr );
    uint8_t i = 0;
    for(i = 1; i < enc_size; i++) {
    	usnprintf(&key_string[i * 2 - 2], 3, "%02X", enc_factory_data[i]);
    }
    LOGF( "\nfactory key: %s\n", key_string);


#if 0 //todo DVT disable!
    for(i = 0; i < AES_BLOCKSIZE+DEVICE_ID_SZ+SHA1_SIZE+3; i++) {
    	usnprintf(&key_string[i * 2], 3, "%02X", factory_data[i]);
    }
    UARTprintf( "\ndec aes: %s\n", key_string);
#endif

	return 0;
}
#ifdef BUILD_TESTS
int Cmd_test_network(int argc,char * argv[]) {
	TestNetwork_RunTests(TEST_SERVER);

	return 0;
}
#endif

#define GPIO_PORT 0x40004000
#define RTC_INT_PIN 0x80
#define GSPI_INT_PIN 0x40
#define NORDIC_INT_GPIO 6
#define PROX_INT_GPIO 7

xSemaphoreHandle spi_smphr;

void nordic_prox_int() {
    unsigned int status;
    signed long xHigherPriorityTaskWoken; //todo this should be basetype_t

	traceISR_ENTER();

    //check for which pin triggered
    status = GPIOIntStatus(GPIO_PORT, FALSE);
	//clear all interrupts

    MAP_GPIOIntClear(GPIO_PORT, status);
	if (status & GSPI_INT_PIN) {
#if DEBUG_PRINT_NORDIC == 1
		LOGI("nordic interrupt\r\n");
#endif
		xSemaphoreGiveFromISR(spi_smphr, &xHigherPriorityTaskWoken);
		MAP_GPIOIntDisable(GPIO_PORT,GSPI_INT_PIN);
	    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
	if (status & RTC_INT_PIN) {
		LOGI("prox interrupt\r\n");
		MAP_GPIOIntDisable(GPIO_PORT,RTC_INT_PIN);
	}
	/* If xHigherPriorityTaskWoken was set to true you
    we should yield.  The actual macro used here is
    port specific. */

	traceISR_EXIT();
}

#include "gpio.h"
#include "gpio_if.h"

void SetupGPIOInterrupts() {
    unsigned char pin;
    unsigned int port;

    port = GPIO_PORT;
    pin = /*RTC_INT_PIN |*/ GSPI_INT_PIN;
#if !ONLY_MID
	GPIO_IF_ConfigureNIntEnable( port, pin, GPIO_HIGH_LEVEL, nordic_prox_int );
	//only one interrupt per port...
#endif
}

void thread_spi(void * data) {
	Cmd_spi_read(0, 0);
	while(1) {
		if(is_top_in_dfu()){
			vTaskDelay(500);
			continue;
		}
		xSemaphoreTake(spi_smphr, 500);
		Cmd_spi_read(0, 0);
		MAP_GPIOIntEnable(GPIO_PORT,GSPI_INT_PIN);
	}

	/*
	while(1) {
		vTaskDelay(8*10);
		Cmd_spi_read(0, 0);
	}
	*/
}



#include "top_hci.h"

int Cmd_slip(int argc, char * argv[]){
	uint32_t len;
	if(argc >= 2){
		uint8_t * message = hci_encode((uint8_t*)argv[1], strlen(argv[1]) + 1, &len);
		LOGF("Decoded: %s \r\n", hci_decode(message, len, NULL));
		hci_free(message);
	}else{
		uint8_t * message = hci_encode("hello", strlen("hello") + 1, &len);
		LOGF("Decoded: %s \r\n", hci_decode(message, len, NULL));
		hci_free(message);
	}
	return 0;
}

int Cmd_topdfu(int argc, char *argv[]){
	if(argc > 1){
		return top_board_dfu_begin(argv[1]);
	}
	LOGF("Usage: topdfu $full_path_to_file");
	return -2;
}

static void CreateDirectoryIfNotExist(const char * path) {

	FILINFO finfo;
	FRESULT res;
	FRESULT res2;


	res = hello_fs_stat(path,&finfo);

	if (res != FR_OK) {
		res2 = hello_fs_mkdir(path);

		if (res2 == FR_OK) {
			LOGI("Created directory %s\r\n",path);
		}
		else {
			LOGI("Failed to create %s\r\n",path);
		}
	}
	else {
		LOGI("%s already exists\r\n",path);
	}

}
static void CreateDefaultDirectories(void) {
	CreateDirectoryIfNotExist("/usr");
}

static int Cmd_test_3200_rtc(int argc, char*argv[]) {
    unsigned int dly = atoi(argv[1]);
	if( argc != 2 ) {
		dly = 3000;
	}
	set_sl_time(0);
	LOGF("time is %u\n", get_sl_time() );
	vTaskDelay(dly);
	LOGF("time is %u\n", get_sl_time() );
	return 0;
}

//#define FILE_TEST
#ifdef FILE_TEST
static int Cmd_generate_user_testing_files(int argc, char* argv[])
{
	CreateDirectoryIfNotExist("/usr");
	int i = 0;
	for(i = 0; i < 5; i++)
	{
		FIL file = {0};

		char file_name[64] = {0};
		usnprintf(file_name, 11, "/usr/test%d", i);
		FRESULT res = hello_fs_open(&file, file_name, FA_WRITE|FA_CREATE_ALWAYS);
		if(res != FR_OK)
		{
			LOGI("Cannot create file test%d\n", i);
			continue;
		}
		char* buffer = {0};
		WORD written = 0;
		hello_fs_write(&file, buffer, 1, &written);  // dummy?
		res = hello_fs_write(&file, buffer, 1, &written);
		if(res != FR_OK)
		{
			LOGI("Write failed\n");
		}
		hello_fs_close(&file);
	}
	return 0;
}
#endif

#include "fault.h"
static void checkFaults() {
    faultInfo f;
    memcpy( (void*)&f, SHUTDOWN_MEM, sizeof(f) );
    if( f.magic == SHUTDOWN_MAGIC ) {
        faultPrinter(&f);
    }
}

void init_download_task( int stack );

void launch_tasks() {
	checkFaults();

	//dear future chris: this one doesn't need a semaphore since it's only written to while threads are going during factory test boot
	booted = true;

	xTaskCreate(thread_fast_i2c_poll, "fastI2CPollTask",  1024 / 4, NULL, 3, NULL);


#ifdef KIT_INCLUDE_FILE_UPLOAD
	xTaskCreate(FileUploaderTask_Thread,"fileUploadTask", 1024/4,NULL,1,NULL);
#endif
#ifdef BUILD_SERVERS //todo PVT disable!
	xTaskCreate(telnetServerTask,"telnetServerTask",512/4,NULL,4,NULL);
	xTaskCreate(httpServerTask,"httpServerTask",3*512/4,NULL,4,NULL);
#endif
	UARTprintf("*");
#if !ONLY_MID
	UARTprintf("*");

	xTaskCreate(thread_dust, "dustTask", 512 / 4, NULL, 3, NULL);
	UARTprintf("*");
	xTaskCreate(thread_sensor_poll, "pollTask", 768 / 4, NULL, 2, NULL);
	UARTprintf("*");
	xTaskCreate(thread_tx, "txTask", 1024 / 4, NULL, 1, NULL);
	UARTprintf("*");
	long_poll_task_init( 2560 / 4 );
	downloadmanagertask_init(3072 / 4);


#endif
}


int Cmd_boot(int argc, char *argv[]) {
	if( !booted ) {
		launch_tasks();
		Cmd_led_clr(0,0);
	}
	return 0;
}


#include "rom.h"
#include "rom_map.h"
#include "hw_adc.h"
#include "adc.h"
int Cmd_inttemp(int argc, char *argv[]) {
	unsigned long ulSample;
	int i;
// Initialize Intercept Temperature
	int g_EfuseInterceptTemperature = 3000;

// Variables for Slope and Intercept
	int temperature, slope_ch3, intcept_ch3;

	//make sure the dust sensor is not using the ADC
	if( !xSemaphoreTake(dust_smphr, 0) ) {
		return 0;
	}

// Enable ADC
	HWREG(ADC_BASE + 0xB8) = 0x0355AA00;
	MAP_ADCEnable(ADC_BASE);

//Initialize slope (for nominal devices)
	slope_ch3 = 204795;

//Get the RAW ADC intercept values from the EFUSE for this particular device.
	intcept_ch3 = (HWREG(0x4402D448) & 0x3FFF) >> 2;

//Read ADC FIFO - Suggested averaging for 4 samples

	/* Wait for the FIFO to fill up */
	vTaskDelay(1);

	temperature = 0;
	for( i=0; i<1000; ++i ) {
		while( !(HWREG(0x4402E8A0) & 0x7) ) {}
		if ((HWREG(0x4402E8A0) & 0x7)) {
			ulSample = (uint16_t) (HWREG(0x4402E880));
			ulSample = (ulSample >> 2) & 0x0FFF;
			temperature += g_EfuseInterceptTemperature
					+ (((intcept_ch3 - (int) ulSample) * slope_ch3 * 100) >> 20);

		}
	}
	MAP_ADCDisable(ADC_BASE);

	xSemaphoreGive(dust_smphr);

	LOGF("internal %u\n", temperature/i);
	return 0;
}

int Cmd_get_gesture_count(int argc, char * argv[]) {

	const int count = gesture_get_and_reset_all_diagnostic_counts();

	LOGF("%d transitions\n",count);

	return 0;
}

int Cmd_sync(int argc, char *argv[]) {
	force_data_push();
	return 0;
}

int cmd_memfrag(int argc, char *argv[]) {
	static void * ptr;
	if (strstr(argv[1], "a") != 0) {
		ptr = pvPortMalloc(atoi(argv[2]));
	}
	else if (strstr(argv[1], "f") != 0) {
		vPortFree(ptr);
	}
	return 0;
}
static long always_slow(int dly){
	vTaskDelay(dly);
	return 0;
}

void
vAssertCalled( const char * s );
int Cmd_fault(int argc, char *argv[]) {
	//*(volatile int*)0xFFFFFFFF = 0xdead;
	//vAssertCalled("test");
	assert(false);
	return 0;
}
int Cmd_fault_slow(int argc, char * argv[]) {
	//SL_SYNC(always_slow(atoi(argv[1])));
	return 0;
}
int Cmd_test_realloc(int argc, char *argv[]) {
	static void * test = NULL;

	LOGI("\n\nrealloc %d\n", atoi(argv[1]));
	test = pvPortRealloc( test, atoi(argv[1]) );
	return 0;
}

int Cmd_heapviz(int argc, char *argv[]) {
	int i;
	#define VIZ_LINE 2500
    char line[VIZ_LINE];

	memset(line, 'x', VIZ_LINE);
	pvPortHeapViz( line, VIZ_LINE );

	LOGF("|");
	for(i=1;i<VIZ_LINE-1;++i) {
		LOGF("%c", line[i]);
		vTaskDelay(1);
	}
	LOGF("|\r\n");

	return 0;
}
int Cmd_disableInterrupts(int argc, char *argv[]) {
	int counts = atoi(argv[1]);
	unsigned long ulInt;
	ulInt = MAP_IntMasterDisable();
	UtilsDelay(counts);
	if (!ulInt) {
		MAP_IntMasterEnable();
	}
	return 0;
}

int Cmd_uptime(int argc, char *argv[]) {
	LOGF("uptime %d\n", xTaskGetTickCount());
	return 0;
}

#define ARR_LEN(x) (sizeof(x)/sizeof(x[0]))
static void print_nwp_version() {
	SlDeviceVersion_t ver;
	uint8_t pConfigOpt;
	short pConfigLen, i;

	pConfigOpt = SL_DEVICE_GENERAL_VERSION;

	pConfigLen = sizeof(SlDeviceVersion_t);

	if( 0 == sl_WlanGet(SL_DEVICE_GENERAL,&pConfigOpt,&pConfigLen,(uint8_t *)(&ver)) ) {
		LOGI("FW " );
		for(i=0;i<ARR_LEN(ver.FwVersion);++i) {
			LOGI("%d.", ver.FwVersion[i] );
		} LOGI("\n");

		LOGI("PHY " );
		for(i=0;i<ARR_LEN(ver.PhyVersion);++i) {
			LOGI("%d.", ver.PhyVersion[i] );
		} LOGI("\n");

		LOGI("CHIP %x\n", ver.ChipId );
		LOGI("NWP " );
		for(i=0;i<ARR_LEN(ver.NwpVersion);++i) {
			LOGI("%d.", ver.NwpVersion[i] );
		} LOGI("\n");

		LOGI("ROM %x\n", ver.RomVersion );
	}
}
int Cmd_nwpinfo(int argc, char *argv[]) {
	print_nwp_version();
	return 0;
}
int Cmd_SyncID(int argc, char * argv[]);
int Cmd_time_test(int argc, char * argv[]);
int cmd_file_sync_upload(int argc, char *argv[]);

int Cmd_read_temp_humid_old(int argc, char *argv[]);
int Cmd_read_uv(int argc, char *argv[]);
int Cmd_uvr(int argc, char *argv[]);
int Cmd_uvw(int argc, char *argv[]);


int cmd_button(int argc, char *argv[]) {

#define LED_GPIO_BASE_DOUT GPIOA2_BASE
#define LED_GPIO_BIT_DOUT 0x80
	bool fast = MAP_GPIOPinRead(LED_GPIO_BASE_DOUT, LED_GPIO_BIT_DOUT);

LOGF("button %d\n", fast);
}
int Cmd_readlight(int argc, char *argv[]);
// ==============================================================================
// This is the table that holds the command names, implementing functions, and
// brief description.
// ==============================================================================
tCmdLineEntry g_sCmdTable[] = {
		//    { "cpu",      Cmd_cpu,      "Show CPU utilization" },
    { "b",      cmd_button,      " " },

#if 0
		{ "time_test", Cmd_time_test, "" },
		{ "heapviz", Cmd_heapviz, "" },
		{ "realloc", Cmd_test_realloc, "" },

		{ "mac", Cmd_set_mac, "" },
		{ "aes", Cmd_set_aes, "" },
#endif
		{ "hwver", Cmd_hwver, "" },

		{ "fault", Cmd_fault, "" },
		{ "faults", Cmd_fault_slow, ""},

		{ "free", Cmd_free, "" },
		{ "connect", Cmd_connect, "" },
		{ "disconnect", Cmd_disconnect, "" },

		{ "ping", Cmd_ping, "" },
		{ "time", Cmd_time, "" },
		{ "status", Cmd_status, "" },

	    { "light",      Cmd_readlight,      "" },
	    { "mnt",      Cmd_mnt,      "" },
    { "umnt",     Cmd_umnt,     "" },
    { "ls",       Cmd_ls,       "" },
    { "chdir",    Cmd_cd,       "" },
    { "cd",       Cmd_cd,       "" },
    { "rm",       Cmd_rm,       "" },
#if 0
    { "mkdir",    Cmd_mkdir,    "" },
    { "write",    Cmd_write,    "" },
    { "mkfs",     Cmd_mkfs,     "" },
    { "pwd",      Cmd_pwd,      "" },

    { "cat",      Cmd_cat,      "" },
	{"codec_Mic", get_codec_mic_NAU, "" }, // TODO DKH

#endif

    {"inttemp", Cmd_inttemp, "" }, //internal temperature
	{ "thp", Cmd_read_temp_hum_press,	"" },
	{ "tv", Cmd_meas_TVOC,	"" },

	{ "uv", Cmd_read_uv, "" },
	{ "light", Cmd_readlight, "" },
#if 1
    {"th-old", Cmd_read_temp_humid_old, "" },
	{ "uvr", Cmd_uvr, "" },
	{ "uvw", Cmd_uvw, "" },
#endif

#if ( configUSE_TRACE_FACILITY == 1 )
		{ "tasks", Cmd_tasks, "" },
#endif

		{ "dust", Cmd_dusttest, "" },
#if 0
		{ "dig", Cmd_dig, "" },
		{ "fswr", Cmd_fs_write, "" }, //serial flash commands
		{ "fsrd", Cmd_fs_read, "" },
		{ "fsdl", Cmd_fs_delete, "" },
		{ "get", Cmd_test_get, ""},
#endif

		{ "pb", Cmd_pbstr, ""},
		{ "hmac", Cmd_testhmac, ""},


		{ "r", Cmd_AudioCapture,""}, //record sounds into SD card
		{ "s",Cmd_audio_record_stop,""},
		{ "x", Cmd_stream_transfer, ""},
		{ "p", Cmd_AudioPlayback, ""},
		{ "getoct",Cmd_get_octogram,""},
		{ "aon",Cmd_audio_turn_on,""},
#if 0
		{ "mode", Cmd_mode, "" }, //set the ap/station mode

		{ "spird", Cmd_spi_read,"" },
		{ "spiwr", Cmd_spi_write, "" },
		{ "spirst", Cmd_spi_reset, "" },
#endif
		{ "antsel", Cmd_antsel, "" }, //select antenna
		{ "led", Cmd_led, "" },
		{ "clrled", Cmd_led_clr, "" },
#ifdef BUILD_RADIO_TEST
		{ "rdiostats", Cmd_RadioGetStats, "" },
		{ "rdiotxstart", Cmd_RadioStartTX, "" },
		{ "rdiotxstop", Cmd_RadioStopTX, "" },
		{ "rdiorxstart", Cmd_RadioStartRX, "" },
		{ "rdiorxstop", Cmd_RadioStopRX, "" },
#endif
#if 0
		{ "slip", Cmd_slip, "" },
#endif
		{ "^", Cmd_send_top, ""}, //send command to top board
		{ "topdfu", Cmd_topdfu, ""}, //update topboard firmware.
		{ "factory_reset", Cmd_factory_reset, ""},//Factory reset from middle.
#if 2
		{ "dtm", Cmd_top_dtm, "" },//Sends Direct Test Mode command
#endif
		{ "animate", Cmd_led_animate, ""},//Animates led

#if 0
		{ "uplog", Cmd_log_upload, ""},
#endif
		{ "loglevel", Cmd_log_setview, "" },
		{ "ver", Cmd_version, ""},//Animates led
#ifdef BUILD_TESTS
		{ "test_network",Cmd_test_network,""},
#endif
		{ "genkey",Cmd_generate_factory_data,""},
		{ "testkey", Cmd_test_key, ""},
		{ "lfclktest",Cmd_test_3200_rtc,""},
		{ "country",Cmd_country,""},
		{ "up",Cmd_uptime,""},
		{ "sync", Cmd_sync, "" },
		{ "boot",Cmd_boot,""},
		{ "gesture_count",Cmd_get_gesture_count,""},
		{ "alarm",set_test_alarm,""},
		{ "set-time",cmd_set_time,""},
		{ "rssi", Cmd_rssi, "" },
		{"dev", Cmd_setDev, ""},
#if 0
		{ "frag",cmd_memfrag,""},
		{ "burntopkey",Cmd_burn_top,""},
		{ "scan",Cmd_scan_wifi,""},
		{"future",Cmd_FutureTest,""},
		{"ana", Cmd_analytics, ""},
		{"noint", Cmd_disableInterrupts, ""},
#endif
		{"resync", Cmd_SyncID, ""},
		{"nwp", Cmd_nwpinfo, ""},

		{"g", Cmd_gesture, ""},
#ifdef BUILD_IPERF
		{ "iperfsvr",Cmd_iperf_server,""},
		{ "iperfcli",Cmd_iperf_client,""},
#endif
#ifdef FILE_TEST
		{ "test_files",Cmd_generate_user_testing_files,""},
#endif
		{"fs", cmd_file_sync_upload, ""},
		{ 0, 0, 0 } };


// ==============================================================================
// This is the UARTTask.  It handles command lines received from the RX IRQ.
// ==============================================================================
void SDHostIntHandler();
extern xSemaphoreHandle g_xRxLineSemaphore;

void UARTStdioIntHandler(void);
long nwp_reset();

void vUARTTask(void *pvParameters) {
	char cCmdBuf[512];
	bool on_charger = false;
	if(led_init() != 0){
		LOGI("Failed to create the led_events.\n");
	}
	xTaskCreate(led_task, "ledTask", 700 / 4, NULL, 3, NULL);
	xTaskCreate(led_idle_task, "led_idle_task", 256 / 4, NULL, 2, NULL);

	//switch the uart lines to gpios, drive tx low and see if rx goes low as well
    // Configure PIN_57 for GPIOInput
    //
    MAP_PinTypeGPIO(PIN_57, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA0_BASE, 0x4, GPIO_DIR_MODE_IN);
    //
    // Configure PIN_55 for GPIOOutput
    //
    MAP_PinTypeGPIO(PIN_55, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA0_BASE, 0x2, GPIO_DIR_MODE_OUT);
    MAP_GPIOPinWrite(GPIOA0_BASE, 0x2, 0);

	//drive sop2 high so we connect UART...
    MAP_GPIOPinWrite(GPIOA3_BASE, 0x2, 0x2);
    vTaskDelay(10);
    if( MAP_GPIOPinRead(GPIOA0_BASE, 0x4) == 0 ) {
    	//drive sop2 low so we disconnect
        MAP_GPIOPinWrite(GPIOA3_BASE, 0x2, 0);
        on_charger = true;
    }
    MAP_PinTypeUART(PIN_55, PIN_MODE_3);
    MAP_PinTypeUART(PIN_57, PIN_MODE_3);

	//
	// Initialize the UART for console I/O.
	//
	UARTStdioInit(0);

	UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);

	UARTprintf("Boot\n");

	UARTprintf("*");
	sl_sync_init();  // thread safe for all sl_* calls

	sl_mode = sl_Start(NULL, NULL, NULL);
	UARTprintf("*");
	while (sl_mode != ROLE_STA) {
		UARTprintf("+");
		sl_WlanSetMode(ROLE_STA);
		sl_mode = nwp_reset();
		UARTprintf("mode switch\r\n");
	}
	UARTprintf("*");

	//default to PCB_ANT
	antsel(get_default_antenna());

	// Set connection policy to Auto, fast
	sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, SL_WLAN_CONNECTION_POLICY(1, 1, 0, 0), NULL, 0);

	UARTprintf("*");

	unsigned char mac[6];
	unsigned short mac_len;
	sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET, NULL, &mac_len, mac);
	UARTprintf("*");

	sl_NetAppStop(0x1f);
	check_hw_version();
	PinMuxConfig_hw_dep();

	// SDCARD INITIALIZATION
	// Enable MMCHS, Reset MMCHS, Configure MMCHS, Configure card clock, mount
	hello_fs_init(); //sets up thread safety for accessing the file system
	MAP_PRCMPeripheralClkEnable(PRCM_SDHOST, PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralReset(PRCM_SDHOST);
	MAP_SDHostInit(SDHOST_BASE);
	// Initialize the DMA Module
	UDMAInit();
	//sdhost dma interrupts
	MAP_SDHostIntRegister(SDHOST_BASE, SDHostIntHandler);
	MAP_SDHostSetExpClk(SDHOST_BASE, MAP_PRCMPeripheralClockGet(PRCM_SDHOST), 24000000);

	UARTprintf("*");
	Cmd_mnt(0, 0);
	vTaskDelay(10);
	//INIT SPI
	spi_init();
	hlo_audio_init();

	i2c_smphr = xSemaphoreCreateRecursiveMutex();

	init_time_module(2560);


	// Init sensors
	init_tvoc();
	init_temp_sensor();
	init_light_sensor();

	init_led_animation();

	data_queue = xQueueCreate(MAX_PERIODIC_DATA, sizeof(periodic_data));
	force_data_queue = xQueueCreate(1, sizeof(periodic_data));
	pill_queue = xQueueCreate(MAX_PILL_DATA, sizeof(pill_data));
	pill_prox_queue = xQueueCreate(MAX_PILL_DATA, sizeof(pill_data));
	vSemaphoreCreateBinary(dust_smphr);
	vSemaphoreCreateBinary(light_smphr);
	vSemaphoreCreateBinary(spi_smphr);
	alarm_smphr = xSemaphoreCreateRecursiveMutex();


	if (data_queue == 0) {
		UARTprintf("Failed to create the data_queue.\n");
	}
	UARTprintf("*");
	SetupGPIOInterrupts();
	CreateDefaultDirectories();
	load_data_server();

	/*******************************************************************************
	*           AUDIO INIT START
	********************************************************************************
	*/

	// Reset codec
	MAP_GPIOPinWrite(GPIOA3_BASE, 0x4, 0);
	vTaskDelay(10);
	MAP_GPIOPinWrite(GPIOA3_BASE, 0x4, 0x4);


#ifdef CODEC_1P5_TEST
	codec_test_commands();
#endif

	// Program codec
	codec_init();

	// McASP and DMA init
	InitAudioTxRx(AUDIO_CAPTURE_PLAYBACK_RATE);

	// Create audio tasks for playback and record
	xTaskCreate(AudioPlaybackTask,"playbackTask",1280/4,NULL,4,NULL);
	xTaskCreate(AudioCaptureTask,"captureTask", (3*1024)/4,NULL,3,NULL);

	xTaskCreate(AudioProcessingTask_Thread,"audioProcessingTask",1*1024/4,NULL,2,NULL);


	/*******************************************************************************
	*           AUDIO INIT END
	********************************************************************************
	*/

	UARTprintf("*");

	init_download_task( 3072 / 4 );


	networktask_init(3 * 1024 / 4);

	load_serial();
	load_aes();
	load_device_id();
	load_account_id();
	load_alarm();
	pill_settings_init();
	check_provision();

	init_dust();

	ble_proto_init();
	xTaskCreate(top_board_task, "top_board_task", 1280 / 4, NULL, 3, NULL);
	xTaskCreate(thread_spi, "spiTask", 1536 / 4, NULL, 3, NULL);


#ifndef BUILD_SERVERS
	uart_logger_init();
	xTaskCreate(uart_logger_task, "logger task",   UART_LOGGER_THREAD_STACK_SIZE/ 4 , NULL, 1, NULL);
	UARTprintf("*");
	xTaskCreate(analytics_event_task, "analyticsTask", 1024/4, NULL, 1, NULL);
	UARTprintf("*");
#endif

	xTaskCreate(thread_alarm, "alarmTask", 1024 / 4, NULL, 2, NULL);
	UARTprintf("*");
	start_top_boot_watcher();

	if( on_charger ) {
		launch_tasks();
		vTaskDelete(NULL);
		return;
	} else {
		play_led_wheel( 50, LED_MAX, LED_MAX, 0,0,10,1);
	}

	UARTprintf("\n\nFreeRTOS %s, %08x, %s %02x:%02x:%02x:%02x:%02x:%02x\n",
	tskKERNEL_VERSION_NUMBER, KIT_VER, MORPH_NAME, mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
	print_nwp_version();
	UARTprintf("> ");

	/* remove anything we recieved before we were ready */

	/* Loop forever */
	while (1) {
		/* Wait for a signal indicating we have an RX line to process */
		xSemaphoreTake(g_xRxLineSemaphore, portMAX_DELAY);

		if (UARTPeek('\r') != -1) {
			/* Read data from the UART and process the command line */
			memset( cCmdBuf, 0, sizeof(cCmdBuf) );
			UARTgets(cCmdBuf, sizeof(cCmdBuf));
			if (ustrlen(cCmdBuf) == 0) {
				LOGI("> ");
				continue;
			}

			//
			// Pass the line from the user to the command processor.  It will be
			// parsed and valid commands executed.
			//
			char * args = NULL;
			args = pvPortMalloc( sizeof(cCmdBuf) );
			if( args == NULL ) {
				LOGF("can't run %s, no mem!\n", cCmdBuf );
			} else {
				memcpy( args, cCmdBuf, sizeof( cCmdBuf ) );
				xTaskCreate(CmdLineProcess, "commandTask",  3*1024 / 4, args, 3, NULL);
			}
        }
	}
}
