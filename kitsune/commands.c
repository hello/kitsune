//*****************************************************************************
//
// commands.c - FreeRTOS porting example on CCS4
//
// Copyright (c) 2012 Fuel7, Inc.
//
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include <assert.h>

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
#include "ti_codec.h"
#include "network.h"

#include "diskio.h"
#include "top_hci.h"
#include "slip_packet.h"
#include "ble_cmd.h"
#include "led_cmd.h"
#include "led_animations.h"
#include "uart_logger.h"

#include "kitsune_version.h"
#include "TestNetwork.h"
#include "sys_time.h"
#include "sl_sync_include_after_simplelink_header.h"

#include "fileuploadertask.h"

#define ONLY_MID 0

//******************************************************************************
//			        FUNCTION DECLARATIONS
//******************************************************************************

extern int g_iReceiveCount;
//******************************************************************************
//			    GLOBAL VARIABLES
//******************************************************************************

tCircularBuffer *pTxBuffer;
tCircularBuffer *pRxBuffer;

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
	LOGI("%d bytes free\nhigh: %d low: %d\n", xPortGetFreeHeapSize(),heap_high_mark,heap_low_mark);

    heap_high_mark = 0;
	heap_low_mark = 0xffffffff;
	heap_print = atoi(argv[1]);
	// Return success.
	return (0);
}


#include "fs.h"

int Cmd_fs_write(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	unsigned long tok=0;
	long hndl, bytes;
	SlFsFileInfo_t info;

	sl_FsGetInfo((unsigned char*)argv[1], tok, &info);

	if (sl_FsOpen((unsigned char*)argv[1],
	FS_MODE_OPEN_WRITE, &tok, &hndl)) {
		LOGI("error opening file, trying to create\n");

		if (sl_FsOpen((unsigned char*)argv[1],
				FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok,
				&hndl)) {
			LOGI("error opening for write\n");
			return -1;
		}
	}

	bytes = sl_FsWrite(hndl, info.FileLen, (unsigned char*)argv[2], strlen(argv[2]));
	LOGI("wrote to the file %d bytes\n", bytes);

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

	sl_FsGetInfo((unsigned char*)argv[1], tok, &info);

	err = sl_FsOpen((unsigned char*) argv[1], FS_MODE_OPEN_READ, &tok, &hndl);
	if (err) {
		LOGI("error opening for read %d\n", err);
		return -1;
	}
	if( argc >= 3 ){
		bytes = sl_FsRead(hndl, atoi(argv[2]), (unsigned char*) buffer,
				minval(info.FileLen, BUF_SZ));
		if (bytes) {
						LOGI("read %d bytes\n", bytes);
					}
	}else{
		bytes = sl_FsRead(hndl, 0, (unsigned char*) buffer,
				minval(info.FileLen, BUF_SZ));
		if (bytes) {
						LOGI("read %d bytes\n", bytes);
					}
	}


	sl_FsClose(hndl, 0, 0, 0);

	for (i = 0; i < bytes; ++i) {
		LOGI("%x", buffer[i]);
	}

	// Return success.
	return (0);
}

#define AUDIO_RATE 16000
extern
unsigned short * audio_buf;
int Cmd_code_playbuff(int argc, char *argv[]) {
unsigned int CPU_XDATA = 1; //1: enabled CPU interrupt triggerred
#define minval( a,b ) a < b ? a : b
	unsigned long tok;
	long hndl, err, bytes;

    McASPInit(true, AUDIO_RATE);
	//Audio_Stop();
	audio_buf = (unsigned short*)pvPortMalloc(AUDIO_BUF_SZ);
	//assert(audio_buf);
	err = sl_FsOpen("Ringtone_hello_leftchannel_16PCM", FS_MODE_OPEN_READ, &tok, &hndl);
	if (err) {
		LOGI("error opening for read %d\n", err);
		return -1;
	}
	bytes = sl_FsRead(hndl, 0,  (unsigned char*)audio_buf, AUDIO_BUF_SZ);
	if (bytes) {
		LOGI("read %d bytes\n", bytes);
	}
	sl_FsClose(hndl, 0, 0, 0);

	get_codec_NAU(atoi(argv[1]));
	LOGI(" Done for get_codec_NAU\n ");

	AudioCaptureRendererConfigure(I2S_PORT_CPU, AUDIO_RATE);

	AudioCapturerInit(CPU_XDATA, AUDIO_RATE); //LOGI(" Done for AudioCapturerInit\n ");

	Audio_Start(); //LOGI(" Done for Audio_Start\n ");

	vTaskDelay(5 * 1000);
	Audio_Stop(); // added this, the ringtone will not play
	McASPDeInit(true, AUDIO_RATE);

	vPortFree(audio_buf); //LOGI(" audio_buf\n ");

	return 0;
}



int Cmd_record_buff(int argc, char *argv[]) {
	AudioMessage_t m;

	//turn on
	memset(&m,0,sizeof(m));
	m.command = eAudioCaptureTurnOn;
	AudioTask_AddMessageToQueue(&m);

	//capture
	memset(&m,0,sizeof(m));
	m.command = eAudioSaveToDisk;
	m.message.capturedesc.captureduration = 625; //about 10 seconds at 62.5 hz
	AudioTask_AddMessageToQueue(&m);

	return 0;

}

int Cmd_audio_turn_on(int argc, char * argv[]) {

	AudioTask_StartCapture();
	return 0;
	}

int Cmd_audio_turn_off(int agrc, char * agrv[]) {
	AudioTask_StopCapture();
	return 0;

}

void Speaker1(char * file);
unsigned char g_ucSpkrStartFlag;

/*
int play_ringtone(int vol, char * file) {

	unsigned int CPU_XDATA = 0; //1: enabled CPU interrupt triggerred; 0: DMA

	{ //naked block so the compiler can pop these off the stack...
	  //check the file exists and is bug enough for a few seconds
		FIL fp;
		FILINFO file_info;
		FRESULT res;
		res = f_open(&fp, file, FA_READ);

		if (res != FR_OK) {
			LOGI("Failed to open audio file %d\n\r", res);
			return -1;
		}
		f_stat(file, &file_info);
		f_close( &fp );
		if (file_info.fsize < 256000) {
			LOGI("audio file too small %d\n\r", file_info.fsize );
			return -1;
		}
	}

//
// Create RX and TX Buffer
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
	AudioProcessingTask_FreeBuffers();
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
	pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
	if (pRxBuffer == NULL) {
		LOGI("Unable to Allocate Memory for Rx Buffer\n\r");
		return -1;
	}
// Configure Audio Codec
//

	get_codec_NAU(vol);

// Initialize the Audio(I2S) Module
//
	AudioCapturerInit(CPU_XDATA, 48000);

// Initialize the DMA Module
//
	UDMAInit();
	UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);

//
// Setup the DMA Mode
//
	SetupPingPongDMATransferRx();
// Setup the Audio In/Out
//
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure(I2S_PORT_DMA, 48000);

	g_ucSpkrStartFlag = 1;
// Start Audio Tx/Rx
//
	Audio_Start();

// Start the Microphone Task
//
	Speaker1(file);
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	LOGI("g_iReceiveCount %d\n\r", g_iReceiveCount);
	close_codec_NAU();
	LOGI("close_codec_NAU");
	Audio_Stop();
	McASPDeInit();
	DestroyCircularBuffer(pRxBuffer);
	LOGI("DestroyCircularBuffer(pRxBuffer)");
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	AudioProcessingTask_AllocBuffers();
	LOGI("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	return 0;

}
*/

int Cmd_stop_buff(int argc, char *argv[]) {
	AudioTask_StopPlayback();

	return 0;
}

int Cmd_play_buff(int argc, char *argv[]) {
    int vol = atoi( argv[1] );
    char * file = argv[2];
    AudioPlaybackDesc_t desc;
    memset(&desc,0,sizeof(desc));
    strncpy( desc.file, file, 64 );
    desc.volume = vol;
    desc.durationInSeconds = 60;

    AudioTask_StartPlayback(&desc);

    return 0;
    //return play_ringtone( vol );
}
int Cmd_fs_delete(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	int err = sl_FsDel((unsigned char*)argv[1], 0);
	if (err) {
		LOGI("error %d\n", err);
		return -1;
	}

	// Return success.
	return (0);
}

static xSemaphoreHandle alarm_smphr;
static SyncResponse_Alarm alarm;
#define ONE_YEAR_IN_SECONDS 0x1E13380

void set_alarm( SyncResponse_Alarm * received_alarm ) {
    if (xSemaphoreTake(alarm_smphr, portMAX_DELAY)) {
        if (received_alarm->has_ring_offset_from_now_in_second
        	&& received_alarm->ring_offset_from_now_in_second > -1 ) {   // -1 means user has no alarm/reset his/her now
        	unsigned long now = get_nwp_time();
        	received_alarm->start_time = now + received_alarm->ring_offset_from_now_in_second;

        	int ring_duration = received_alarm->has_ring_duration_in_second ? received_alarm->ring_duration_in_second : 30;
        	received_alarm->end_time = now + received_alarm->ring_offset_from_now_in_second + ring_duration;
        	received_alarm->has_end_time = received_alarm->has_start_time = received_alarm->has_ring_duration_in_second = true;
        	received_alarm->ring_duration_in_second = ring_duration;
        	// //sanity check
        	// since received_alarm->ring_offset_from_now_in_second >= 0, we don't need to check received_alarm->start_time
            
            //are we within the duration of the current alarm?
            if( alarm.has_start_time
             && alarm.start_time - now > 0
             && now - alarm.start_time < alarm.ring_duration_in_second ) {
                LOGI( "alarm currently active, putting off setting\n");
            } else {
                memcpy(&alarm, received_alarm, sizeof(alarm));
            }
            LOGI("Got alarm %d to %d in %d minutes\n",
                        received_alarm->start_time, received_alarm->end_time,
                        (received_alarm->start_time - now) / 60);
        }else{
            LOGI("No alarm for now.\n");
            // when we reach here, we need to cancel the existing alarm to prevent them ringing.

            // The following is not necessary, putting here just to make them explicit.
            received_alarm->start_time = 0;
            received_alarm->end_time = 0;
            received_alarm->has_start_time = false;
            received_alarm->has_end_time = false;

            memcpy(&alarm, received_alarm, sizeof(alarm));
        }

        xSemaphoreGive(alarm_smphr);
    }
}

static void thread_alarm_on_finished(void * context) {
	if (xSemaphoreTake(alarm_smphr, portMAX_DELAY)) {

		if (alarm.has_end_time) {
			LOGI("ALARM DONE RINGING\n");
			alarm.has_end_time = 0;
			alarm.has_start_time = 0;
        }

        xSemaphoreGive(alarm_smphr);
    }
}

void thread_alarm(void * unused) {
	while (1) {
		portTickType now = xTaskGetTickCount();
		//todo audio processing
		uint64_t time = get_nwp_time();
		// The alarm thread should go ahead even without a valid time,
		// because we don't need a correct time to fire alarm, we just need the offset.

		if (xSemaphoreTake(alarm_smphr, portMAX_DELAY)) {
			if(alarm.has_start_time && alarm.start_time > 0)
			{
				if ( time - alarm.start_time < alarm.ring_duration_in_second ) {
					AudioPlaybackDesc_t desc;
					memset(&desc,0,sizeof(desc));

					strncpy( desc.file, AUDIO_FILE, 64 );
					desc.durationInSeconds = alarm.ring_duration_in_second;
					desc.volume = 57;
					desc.onFinished = thread_alarm_on_finished;

					AudioTask_StartPlayback(&desc);
					LOGI("ALARM RINGING RING RING RING\n");
					alarm.has_start_time = 0;
					alarm.start_time = 0;
				}
			}
			else {
				// Alarm start time = 0 means no alarm
			}
			
			xSemaphoreGive(alarm_smphr);
		}
		vTaskDelayUntil(&now, 1000 );
	}
}

#define SENSOR_RATE 60

static int dust_m2,dust_mean,dust_log_sum,dust_max,dust_min,dust_cnt;
xSemaphoreHandle dust_smphr;

void thread_dust(void * unused)  {
    #define maxval( a, b ) a>b ? a : b
	dust_min = 5000;
	dust_m2 = dust_mean = dust_cnt = dust_log_sum = dust_max = 0;
	while (1) {
		if (xSemaphoreTake(dust_smphr, portMAX_DELAY)) {
			int dust = get_dust();

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

			xSemaphoreGive(dust_smphr);
		}

		vTaskDelay( 100 );
	}
}

static void _on_wave(void * ctx){
	alarm.has_start_time = 0;
	AudioTask_StopPlayback();

	uint8_t adjust_max_light = 80;

	int adjust;
	int light = *(int*)ctx;

				if( light > adjust_max_light ) {
					adjust = adjust_max_light;
				} else {
					adjust = light;
				}

				if(adjust < 20)
				{
					adjust = 20;
				}

				uint8_t alpha = 0xFF * adjust / 80;

				if( sl_status & UPLOADING ) {
					uint8_t rgb[3] = { LED_MAX };
					led_get_user_color(&rgb[0], &rgb[1], &rgb[2]);
					led_set_color(alpha, rgb[0], rgb[1], rgb[2], 1, 1, 18, 0);
			 	}
			 	else if( sl_status & HAS_IP ) {
					led_set_color(alpha, LED_MAX, 0, 0, 1, 1, 18, 1); //blue
			 	}
			 	else if( sl_status & CONNECTING ) {
			 		led_set_color(alpha, LED_MAX,LED_MAX,0, 1, 1, 18, 1); //yellow
			 	}
			 	else if( sl_status & SCANNING ) {
			 		led_set_color(alpha, LED_MAX,0,0, 1, 1, 18, 1 ); //red
			 	} else {
			 		led_set_color(alpha, LED_MAX, LED_MAX, LED_MAX, 1, 1, 18, 1 ); //white
			 	}
			}
static void _on_hold(void * ctx){
	//stop_led_animation();
	alarm.has_start_time = 0;
	AudioTask_StopPlayback();
}
static void _on_slide(void * ctx, int delta){
	LOGI("Slide delta %d\r\n", delta);
}
static int light_m2,light_mean, light_cnt,light_log_sum,light_sf;
static xSemaphoreHandle light_smphr;

 xSemaphoreHandle i2c_smphr;
 int Cmd_led(int argc, char *argv[]) ;
#include "gesture.h"
void thread_fast_i2c_poll(void * unused)  {
	int light = 0;
	gesture_callbacks_t gesture_cbs = (gesture_callbacks_t){
		.on_wave = _on_wave,
		.on_hold = _on_hold,
		.on_slide = _on_slide,
		.ctx = &light,
	};
	gesture_init(&gesture_cbs);
	while (1) {
		portTickType now = xTaskGetTickCount();
		int prox=0;

		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			vTaskDelay(2);
			light = get_light();
			vTaskDelay(2); //this is important! If we don't do it, then the prox will stretch the clock!

			// For the black morpheus, we can detect 6mm distance max
			// for white one, 9mm distance max.
			prox = get_prox();  // now this thing is in um.

			xSemaphoreGive(i2c_smphr);
			gesture_input(prox);

			if (xSemaphoreTake(light_smphr, portMAX_DELAY)) {
				light_log_sum += bitlog(light);
				++light_cnt;

				int delta = light - light_mean;
				light_mean = light_mean + delta/light_cnt;
				light_m2 = light_m2 + delta * ( light - light_mean);
				if( light_m2 < 0 ) {
					light_m2 = 0x7FFFFFFF;
				}

				//LOGI( "%d %d %d %d\n", delta, light_mean, light_m2, light_cnt);
				xSemaphoreGive(light_smphr);
			}
		}
		vTaskDelayUntil(&now, 100);
	}
}

#define MAX_PILL_DATA 20
#define MAX_BATCH_PILL_DATA 10
#define PILL_BATCH_WATERMARK 2

xQueueHandle data_queue = 0;
xQueueHandle pill_queue = 0;

typedef struct {
	pill_data * pills;
	int num_pills;
} pilldata_to_encode;

static bool encode_all_pills (pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	int i;
	pilldata_to_encode * data = *(pilldata_to_encode**)arg;

	for( i = 0; i < data->num_pills; ++i ) {
		if(!pb_encode_tag(stream, PB_WT_STRING, batched_pill_data_pills_tag))
		{
			LOGI("encode_all_pills: Fail to encode tag for pill %s, error %s\n", data->pills[i].device_id, PB_GET_ERROR(stream));
			return false;
		}

		if (!pb_encode_delimited(stream, pill_data_fields, &data->pills[i])){
			LOGI("encode_all_pills: Fail to encode pill %s, error: %s\n", data->pills[i].device_id, PB_GET_ERROR(stream));
			return false;
		}
		//LOGI("******************* encode_pill_encode_all_pills: encode pill %s\n", pill_data.deviceId);
	}
	return true;
}
void thread_tx(void* unused) {
	batched_pill_data pill_data_batched = {0};
	periodic_data data = {0};
	load_aes();

	LOGI(" Start polling  \n");
	while (1) {
		int tries = 0;
		memset(&data, 0, sizeof(periodic_data));
		if (xQueueReceive(data_queue, &(data), 1)) {
			LOGI(
					"sending time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d\n",
					data.unix_time, data.light, data.light_variability,
					data.light_tonality, data.temperature, data.humidity, data.dust);

			while (!send_periodic_data(&data) == 0) {
				LOGI("  Waiting for WIFI connection  \n");
				vTaskDelay((1 << tries) * 1000);
				if (tries++ > 5) {
					tries = 5;
				}
			}
		}

		tries = 0;
		if (uxQueueMessagesWaiting(pill_queue) > PILL_BATCH_WATERMARK) {
			LOGI(	"sending  pill data" );
			pilldata_to_encode pilldata;
			pilldata.num_pills = 0;
			pilldata.pills = (pill_data*)pvPortMalloc(MAX_BATCH_PILL_DATA*sizeof(pill_data));

			if( !pilldata.pills ) {
				LOGI( "failed to alloc pilldata\n" );
				vTaskDelay(1000);
				continue;
			}

			while( pilldata.num_pills < MAX_BATCH_PILL_DATA && xQueueReceive(pill_queue, &pilldata.pills[pilldata.num_pills], 1 ) ) {
				++pilldata.num_pills;
			}

			memset(&pill_data_batched, 0, sizeof(pill_data_batched));
			pill_data_batched.pills.funcs.encode = encode_all_pills;  // This is smart :D
			pill_data_batched.pills.arg = &pilldata;
			pill_data_batched.device_id.funcs.encode = encode_mac_as_device_id_string;

			while (!send_pill_data(&pill_data_batched) == 0) {
				LOGI("  Waiting for WIFI connection  \n");
				vTaskDelay((1 << tries) * 1000);
				if (tries++ > 5) {
					tries = 5;
				}
			}
			vPortFree( pilldata.pills );
		}
		while (!(sl_status & HAS_IP)) {
			vTaskDelay(1000);
		}
	}
}

void thread_sensor_poll(void* unused) {

	//
	// Print some header text.
	//

	periodic_data data = {0};

	while (1) {
		portTickType now = xTaskGetTickCount();

		memset(&data, 0, sizeof(data));  // Don't forget re-init!

		if(!time_module_initialized())  // Initialize sys time module in sensor polling thread
		{
			uint32_t ntp_time = fetch_time_from_ntp_server();
			if(ntp_time != INVALID_SYS_TIME)
			{
				if(set_nwp_time(ntp_time) != INVALID_SYS_TIME)
				{
					init_time_module();
				}
			}
			vTaskDelay(200);
			continue;  // The data polling thread should not proceed without a valid time.
		}

		data.unix_time = get_nwp_time();
		data.has_unix_time = true;

		// copy over the dust values
		if( xSemaphoreTake(dust_smphr, portMAX_DELAY)) {
			if( dust_cnt != 0 ) {
				data.dust = dust_mean;
				data.has_dust = true;

				dust_log_sum /= dust_cnt;  // devide by zero?
				if(dust_cnt > 1)
				{
					data.dust_variability = dust_m2 / (dust_cnt - 1);  // devide by zero again, add if
					data.has_dust_variability = true;  // since init with 0, by default it is false
				}
				data.has_dust_max = true;
				data.dust_max = dust_max;

				data.has_dust_min = true;
				data.dust_min = dust_min;

				
			} else {
				data.dust = get_dust();
				if(data.dust == 0)  // This means we get some error?
				{
					data.has_dust = false;
				}

				data.has_dust_variability = false;
				data.has_dust_max = false;
				data.has_dust_min = false;
			}
			
			dust_min = 5000;
			dust_m2 = dust_mean = dust_cnt = dust_log_sum = dust_max = 0;
			xSemaphoreGive(dust_smphr);
		} else {
			data.has_dust = false;  // if Semaphore take failed, don't upload
		}

		// copy over light values
		if (xSemaphoreTake(light_smphr, portMAX_DELAY)) {
			if(light_cnt == 0)
			{
				data.has_light = false;
			}else{
				light_log_sum /= light_cnt;  // just be careful for devide by zero.
				light_sf = (light_mean << 8) / bitexp( light_log_sum );

				if(light_cnt > 1)
				{
					data.light_variability = light_m2 / (light_cnt - 1);
					data.has_light_variability = true;
				}else{
					data.has_light_variability = false;
				}

				//LOGI( "%d lightsf %d var %d cnt\n", light_sf, light_var, light_cnt );
				data.light_tonality = light_sf;
				data.has_light_tonality = true;

				data.light = light_mean;
				data.has_light = true;

				light_m2 = light_mean = light_cnt = light_log_sum = light_sf = 0;
			}
			
			xSemaphoreGive(light_smphr);
		}

		// get temperature and humidity
		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			uint8_t measure_time = 10;
			int64_t humid_sum = 0;
			int64_t temp_sum = 0;

			uint8_t humid_count = 0;
			uint8_t temp_count = 0;

			while(--measure_time)
			{
				vTaskDelay(2);

				int humid = get_humid();
				if(humid != -1)
				{
					humid_sum += humid;
					humid_count++;
				}

				vTaskDelay(2);

				int temp = get_temp();

				if(temp != -1)
				{
					temp_sum += temp;
					temp_count++;
				}

				vTaskDelay(2);
			}



			if(humid_count == 0)
			{
				data.has_humidity = false;
			}else{
				data.has_humidity = true;
				data.humidity = humid_sum / humid_count;

			}


			if(temp_count == 0)
			{
				data.has_temperature = false;
			}else{
				data.has_temperature = true;
				data.temperature = temp_sum / temp_count;
			}
			
			xSemaphoreGive(i2c_smphr);
		}


		LOGI("collecting time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d %d %d %d\n",
				data.unix_time, data.light, data.light_variability, data.light_tonality, data.temperature, data.humidity,
				data.dust, data.dust_max, data.dust_min, data.dust_variability);

		// Remember to add back firmware version, or OTA cant work.
		data.has_firmware_version = true;
		data.firmware_version = KIT_VER;
        if(!xQueueSend(data_queue, (void*)&data, 10) == pdPASS)
        {
    		LOGI("Failed to post data\n");
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

	LOGI("\t\t\t\t\tUnused\n            TaskName\tStatus\tPri\tStack\tTask ID\n");
	pBuffer = pvPortMalloc(1024);
	assert(pBuffer);
	LOGI("==========================");
	vTaskList(pBuffer);
	LOGI("==========================\n");
	LOGI("%s", pBuffer);

	vPortFree(pBuffer);
	return 0;
}

#endif /* configUSE_TRACE_FACILITY */

// ==============================================================================
// This function implements the "help" command.  It prints a simple list of the
// available commands with a brief description.
// ==============================================================================
int Cmd_help(int argc, char *argv[]) {
	tCmdLineEntry *pEntry;

	//
	// Print some header text.
	//
	LOGI("\nAvailable commands\n");
	LOGI("------------------\n");

	//
	// Point at the beginning of the command table.
	//
	pEntry = &g_sCmdTable[0];

	//
	// Enter a loop to read each entry from the command table.  The end of the
	// table has been reached when the command name is NULL.
	//
	while (pEntry->pcCmd) {
		//
		// Print the command name and the brief description.
		//
		LOGI("%s: %s\n", pEntry->pcCmd, pEntry->pcHelp);

		vTaskDelay(10);
		//
		// Advance to the next entry in the table.
		//
		pEntry++;
	}

	//
	// Return success.
	//
	return (0);
}

int Cmd_fault(int argc, char *argv[]) {
	*(int*) (0x40001) = 1; /* error logging test... */
	return 0;
}


#define SCAN_TABLE_SIZE   20

static void SortByRSSI(Sl_WlanNetworkEntry_t* netEntries,
                                            unsigned char ucSSIDCount)
{
    Sl_WlanNetworkEntry_t tTempNetEntry;
    unsigned char ucCount, ucSwapped;
    do{
        ucSwapped = 0;
        for(ucCount =0; ucCount < ucSSIDCount - 1; ucCount++)
        {
           if(netEntries[ucCount].rssi < netEntries[ucCount + 1].rssi)
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

	Sl_WlanNetworkEntry_t g_netEntries[SCAN_TABLE_SIZE];

	lCountSSID = get_wifi_scan_result(&g_netEntries[0], SCAN_TABLE_SIZE, 1000);

    SortByRSSI(&g_netEntries[0],(unsigned char)lCountSSID);

    LOGI( "SSID RSSI\n" );
	for(i=0;i<lCountSSID;++i) {
		LOGI( "%s %d\n", g_netEntries[i].ssid, g_netEntries[i].rssi );
	}
	return 0;
}

int Cmd_test_network(int argc,char * argv[]) {
	TestNetwork_RunTests(TEST_SERVER);

	return 0;
}
#if 0
int Cmd_mel(int argc, char *argv[]) {
    int i,ichunk;
	int16_t x[1024];


	srand(0);

	LOGI("EXPECT: t1=%d,t2=%d,energy=something not zero\n",43,86);


	AudioFeatures_Init(AudioFeatCallback);

	//still ---> white random noise ---> still
	for (ichunk = 0; ichunk < 43*8; ichunk++) {
		if (ichunk > 43 && ichunk <= 86) {
			for (i = 0; i < 1024; i++) {
				x[i] = (rand() % 32767) - (1<<14);
			}
		}
		else {
			memset(x,0,sizeof(x));
		}

		AudioFeatures_SetAudioData(x,10,ichunk);

	}
	return (0);
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
		if (xSemaphoreTake(spi_smphr, 10000) ) {
			vTaskDelay(8*10);
			Cmd_spi_read(0, 0);
			MAP_GPIOIntEnable(GPIO_PORT,GSPI_INT_PIN);
		} else {
			MAP_GPIOIntEnable(GPIO_PORT,GSPI_INT_PIN);
		}
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
		LOGI("Decoded: %s \r\n", hci_decode(message, len, NULL));
		hci_free(message);
	}else{
		uint8_t * message = hci_encode("hello", strlen("hello") + 1, &len);
		LOGI("Decoded: %s \r\n", hci_decode(message, len, NULL));
		hci_free(message);
	}
	return 0;
}

int Cmd_topdfu(int argc, char *argv[]){
	if(argc > 1){
		return top_board_dfu_begin(argv[1]);
	}
	LOGI("Usage: topdfu $full_path_to_file");
	return -2;
}

// ==============================================================================
// This is the table that holds the command names, implementing functions, and
// brief description.
// ==============================================================================
tCmdLineEntry g_sCmdTable[] = {
		{ "help", Cmd_help, "Display list of commands" },
		{ "?", Cmd_help,"alias for help" },
//    { "cpu",      Cmd_cpu,      "Show CPU utilization" },
		{ "free", Cmd_free, "" },
		{ "connect", Cmd_connect, "" },
		{ "disconnect", Cmd_disconnect, "" },
		{ "mac", Cmd_set_mac, "" },
		{ "aes", Cmd_set_aes, "" },

		{ "ping", Cmd_ping, "" },
		{ "time", Cmd_time, "" },
		{ "status", Cmd_status, "" },
#if 0
		{ "audio", Cmd_audio_test, "audio upload test" },
#endif

    { "mnt",      Cmd_mnt,      "" },
    { "umnt",     Cmd_umnt,     "" },
    { "ls",       Cmd_ls,       "" },
    { "chdir",    Cmd_cd,       "" },
    { "cd",       Cmd_cd,       "" },
    { "mkdir",    Cmd_mkdir,    "" },
    { "rm",       Cmd_rm,       "" },
    { "write",    Cmd_write,    "" },
    { "mkfs",     Cmd_mkfs,     "" },
    { "pwd",      Cmd_pwd,      "" },
    { "cat",      Cmd_cat,      "" },
		//{ "fault", Cmd_fault, "" },

		{ "humid", Cmd_readhumid, "" },
		{ "temp", Cmd_readtemp,	"" },
		{ "light", Cmd_readlight, "" },
		{"prox", Cmd_readproximity, "" },
		{"codec_Mic", get_codec_mic_NAU, "" },

#if ( configUSE_TRACE_FACILITY == 1 )
		{ "tasks", Cmd_tasks, "" },
#endif

		{ "dust", Cmd_dusttest, "" },

		{ "fswr", Cmd_fs_write, "" }, //serial flash commands
		{ "fsrd", Cmd_fs_read, "" },
		{ "fsdl", Cmd_fs_delete, "" },

		{ "play_ringtone", Cmd_code_playbuff, "" },
		{ "stop_ringtone",Cmd_stop_buff,""},
		{ "r", Cmd_record_buff,""}, //record sounds into SD card
		{ "p", Cmd_play_buff, ""},//play sounds from SD card
		{ "aon",Cmd_audio_turn_on,""},
		{ "aoff",Cmd_audio_turn_off,""},
		//{ "readout", Cmd_readout_data, "read out sensor data log" },

		{ "sl", Cmd_sl, "" }, // smart config
		{ "mode", Cmd_mode, "" }, //set the ap/station mode
		//{ "mel", Cmd_mel, "test the mel calculation" },

		{ "spird", Cmd_spi_read,"" },
		{ "spiwr", Cmd_spi_write, "" },
		{ "spirst", Cmd_spi_reset, "" },

		{ "antsel", Cmd_antsel, "" }, //select antenna
		{ "led", Cmd_led, "" },
		{ "clrled", Cmd_led_clr, "" },

		{ "rdiostats", Cmd_RadioGetStats, "" },
		{ "rdiotxstart", Cmd_RadioStartTX, "" },
		{ "rdiotxstop", Cmd_RadioStopTX, "" },
		{ "rdiorxstart", Cmd_RadioStartRX, "" },
		{ "rdiorxstop", Cmd_RadioStopRX, "" },
		{ "rssi", Cmd_rssi, "" },
		{ "slip", Cmd_slip, "" },
		{ "data_upload", Cmd_data_upload, "" },
		{ "^", Cmd_send_top, ""}, //send command to top board
		{ "topdfu", Cmd_topdfu, ""}, //update topboard firmware.
		{ "factory_reset", Cmd_factory_reset, ""},//Factory reset from middle.
		{ "download", Cmd_download, ""},//download test function.
		{ "dtm", Cmd_top_dtm, "" },//Sends Direct Test Mode command
		{ "animate", Cmd_led_animate, ""},//Animates led
		{ "uplog", Cmd_log_upload, "Uploads log to server"},
		{ "loglevel", Cmd_log_setview, "Sets log level" },
		{ "ver", Cmd_version, ""},//Animates led
		{ "test_network",Cmd_test_network,""},

		{ 0, 0, 0 } };

//#include "fault.h"
//static void checkFaults() {
//    faultInfo f;
//    memcpy( (void*)&f, SHUTDOWN_MEM, sizeof(f) );
//    if( f.magic == SHUTDOWN_MAGIC ) {
//        faultPrinter(&f);
//    }
//}

// ==============================================================================
// This is the UARTTask.  It handles command lines received from the RX IRQ.
// ==============================================================================
extern xSemaphoreHandle g_xRxLineSemaphore;
void UARTStdioIntHandler(void);
void init_download_task( int stack );
long nwp_reset();

void vUARTTask(void *pvParameters) {
	char cCmdBuf[512];
	portTickType now;

	if(led_init() != 0){
		LOGI("Failed to create the led_events.\n");
	}

	xTaskCreate(led_task, "ledTask", 512 / 4, NULL, 4, NULL); //todo reduce stack

	Cmd_led_clr(0,0);
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

    vTaskDelay(100);
    if( MAP_GPIOPinRead(GPIOA0_BASE, 0x4) == 0 ) {
    	//drive sop2 low so we disconnect
        MAP_GPIOPinWrite(GPIOA3_BASE, 0x2, 0);
    }
    MAP_PinTypeUART(PIN_55, PIN_MODE_3);
    MAP_PinTypeUART(PIN_57, PIN_MODE_3);

	//
	// Initialize the UART for console I/O.
	//
	UARTStdioInit(0);

	UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);

	LOGI("Boot\n");

	//default to IFA
	antsel(IFA_ANT);

	LOGI("*");
	now = xTaskGetTickCount();
	sl_mode = sl_Start(NULL, NULL, NULL);
	LOGI("*");
	while (sl_mode != ROLE_STA) {
		LOGI("+");
		sl_WlanSetMode(ROLE_STA);
		nwp_reset();
	}
	LOGI("*");

	// Set connection policy to Auto
	sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 0), NULL, 0);

	LOGI("*");
	unsigned char mac[6];
	unsigned char mac_len;
	sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
	LOGI("*");

	// SDCARD INITIALIZATION
	// Enable MMCHS, Reset MMCHS, Configure MMCHS, Configure card clock, mount
	MAP_PRCMPeripheralClkEnable(PRCM_SDHOST, PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralReset(PRCM_SDHOST);
	MAP_SDHostInit(SDHOST_BASE);
	MAP_SDHostSetExpClk(SDHOST_BASE, MAP_PRCMPeripheralClockGet(PRCM_SDHOST),
			1000000);
	LOGI("*");
	Cmd_mnt(0, 0);
	LOGI("*");
	//INIT SPI
	spi_init();
	LOGI("*");

	vTaskDelayUntil(&now, 1000);
	LOGI("*");

	if (sl_mode == ROLE_AP || !sl_status) {
		//Cmd_sl(0, 0);
	}

	// Init sensors
	init_humid_sensor();
	init_temp_sensor();
	init_light_sensor();
	init_prox_sensor();

	init_led_animation();

	data_queue = xQueueCreate(10, sizeof(periodic_data));
	pill_queue = xQueueCreate(MAX_PILL_DATA, sizeof(pill_data));
	vSemaphoreCreateBinary(dust_smphr);
	vSemaphoreCreateBinary(light_smphr);
	vSemaphoreCreateBinary(i2c_smphr);
	vSemaphoreCreateBinary(spi_smphr);
	vSemaphoreCreateBinary(alarm_smphr);


	if (data_queue == 0) {
		LOGI("Failed to create the data_queue.\n");
	}


	init_download_task( 1024 / 4 );
	networktask_init(5 * 1024 / 4);

	xTaskCreate(top_board_task, "top_board_task", 1024 / 4, NULL, 2, NULL);
	xTaskCreate(thread_alarm, "alarmTask", 2*1024 / 4, NULL, 4, NULL);

	LOGI("*");
	xTaskCreate(thread_spi, "spiTask", 3*1024 / 4, NULL, 4, NULL); //this one doesn't look like much, but has to parse all the pb from bluetooth


	xTaskCreate(FileUploaderTask_Thread,"fileUploadTask",1*1024/4,NULL,1,NULL);


	SetupGPIOInterrupts();
	LOGI("*");
#if !ONLY_MID

	xTaskCreate(AudioTask_Thread,"audioTask",3*1024/4,NULL,4,NULL);
	LOGI("*");
	xTaskCreate(AudioProcessingTask_Thread,"audioProcessingTask",1*1024/4,NULL,1,NULL);
	LOGI("*");
	xTaskCreate(thread_fast_i2c_poll, "fastI2CPollTask",  512 / 4, NULL, 4, NULL);
	LOGI("*");
	xTaskCreate(thread_dust, "dustTask", 256 / 4, NULL, 3, NULL);
	LOGI("*");
	xTaskCreate(thread_sensor_poll, "pollTask", 1024 / 4, NULL, 3, NULL);
	LOGI("*");
	xTaskCreate(thread_tx, "txTask", 3 * 1024 / 4, NULL, 2, NULL);
	LOGI("*");
	xTaskCreate(uart_logger_task, "logger task",   UART_LOGGER_THREAD_STACK_SIZE/ 4 , NULL, 4, NULL);
	LOGI("*");
#endif
	//checkFaults();



	LOGI("\n\nFreeRTOS %s, %x, %s %x%x%x%x%x%x\n",
	tskKERNEL_VERSION_NUMBER, KIT_VER, MORPH_NAME, mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
	LOGI("\n? for help\n");
	LOGI("> ");

	/* remove anything we recieved before we were ready */

	/* Loop forever */
	while (1) {
		/* Wait for a signal indicating we have an RX line to process */
		xSemaphoreTake(g_xRxLineSemaphore, portMAX_DELAY);

		if (UARTPeek('\r') != -1) {
			/* Read data from the UART and process the command line */
			UARTgets(cCmdBuf, sizeof(cCmdBuf));
			if (ustrlen(cCmdBuf) == 0) {
				LOGI("> ");
				continue;
			}

			//
			// Pass the line from the user to the command processor.  It will be
			// parsed and valid commands executed.
			//
			char * args = pvPortMalloc( sizeof(cCmdBuf) );
			memcpy( args, cCmdBuf, sizeof( cCmdBuf ) );
			xTaskCreate(CmdLineProcess, "commandTask",  5*1024 / 4, args, 4, NULL);
        }
	}
}
