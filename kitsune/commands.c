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
#include "audiocapturetask.h"
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

#include "ff.h"
#include "diskio.h"
#include "top_hci.h"
#include "slip_packet.h"
#include "ble_cmd.h"
#include "led_cmd.h"
#include "led_animations.h"

#include "tests/TestNetwork.h"

#define ONLY_MID 0

//******************************************************************************
//			        FUNCTION DECLARATIONS
//******************************************************************************

extern int g_iReceiveCount;
//******************************************************************************
//			    GLOBAL VARIABLES
//******************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

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
//    UARTprintf("ARM Cortex-M4F %u MHz - ",configCPU_CLOCK_HZ / 1000000);
//    UARTprintf("%2u%% utilization\n", (g_ulCPUUsage+32768) >> 16);
//
//    // Return success.
//    return(0);
//}

// ==============================================================================
// This function implements the "free" command.  It prints the free memory.
// ==============================================================================
int Cmd_free(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

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
		UARTprintf("error opening file, trying to create\n");

		if (sl_FsOpen((unsigned char*)argv[1],
				FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok,
				&hndl)) {
			UARTprintf("error opening for write\n");
			return -1;
		}
	}

	bytes = sl_FsWrite(hndl, info.FileLen, (unsigned char*)argv[2], strlen(argv[2]));
	UARTprintf("wrote to the file %d bytes\n", bytes);

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

	if (err = sl_FsOpen((unsigned char*)argv[1], FS_MODE_OPEN_READ, &tok, &hndl)) {
		UARTprintf("error opening for read %d\n", err);
		return -1;
	}
	if( argc >= 3 ){
		if (bytes = sl_FsRead(hndl, atoi(argv[2]), (unsigned char*)buffer, minval(info.FileLen, BUF_SZ))) {
						UARTprintf("read %d bytes\n", bytes);
					}
	}else{
		if (bytes = sl_FsRead(hndl, 0, (unsigned char*)buffer, minval(info.FileLen, BUF_SZ))) {
						UARTprintf("read %d bytes\n", bytes);
					}
	}


	sl_FsClose(hndl, 0, 0, 0);

	for (i = 0; i < bytes; ++i) {
		UARTprintf("%x", buffer[i]);
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
	if (err = sl_FsOpen("Ringtone_hello_leftchannel_16PCM", FS_MODE_OPEN_READ, &tok, &hndl)) {
		UARTprintf("error opening for read %d\n", err);
		return -1;
	}
	if (bytes = sl_FsRead(hndl, 0,  (unsigned char*)audio_buf, AUDIO_BUF_SZ)) {
		UARTprintf("read %d bytes\n", bytes);
	}
	sl_FsClose(hndl, 0, 0, 0);

	get_codec_NAU(atoi(argv[1]));
	UARTprintf(" Done for get_codec_NAU\n ");

	AudioCaptureRendererConfigure(I2S_PORT_CPU, AUDIO_RATE);

	AudioCapturerInit(CPU_XDATA, AUDIO_RATE); //UARTprintf(" Done for AudioCapturerInit\n ");

	Audio_Start(); //UARTprintf(" Done for Audio_Start\n ");

	vTaskDelay(5 * 1000);
	Audio_Stop(); // added this, the ringtone will not play
	McASPDeInit(true, AUDIO_RATE);

	vPortFree(audio_buf); //UARTprintf(" audio_buf\n ");

	return 0;
}

static void RecordingCompleteNotification(void) {
	AudioCaptureMessage_t message;

	//turn off
	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOff;
	AudioCaptureTask_AddMessageToQueue(&message);

}

int Cmd_record_buff(int argc, char *argv[]) {
	AudioCaptureMessage_t message;

	//turn on
	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOn;
	AudioCaptureTask_AddMessageToQueue(&message);

	//capture
	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureSaveToDisk;
	message.captureduration = 625; //about 10 seconds at 62.5 hz
	message.fpCommandComplete = RecordingCompleteNotification;
	AudioCaptureTask_AddMessageToQueue(&message);

	return 0;

}

int Cmd_audio_do_stuff(int argc, char * argv[]) {

	AudioCaptureMessage_t message;
	const char * buf = argv[1];

	if (strncmp(buf,"on",4) == 0) {
		//turn on
		UARTprintf("Turning on audio\r\n");
		memset(&message,0,sizeof(message));
		message.command = eAudioCaptureTurnOn;
		AudioCaptureTask_AddMessageToQueue(&message);
	}
	else if (strncmp(buf,"off",4) == 0) {
		//turn on
		UARTprintf("Turning off audio\r\n");

		memset(&message,0,sizeof(message));
		message.command = eAudioCaptureTurnOff;
		AudioCaptureTask_AddMessageToQueue(&message);
	}
	else {
		UARTprintf("aud command takes an argument, either on or off\r\n");
	}

	return 0;
}

void Speaker1(char * file);
unsigned char g_ucSpkrStartFlag;

int play_ringtone(int vol, char * file) {

	unsigned int CPU_XDATA = 0; //1: enabled CPU interrupt triggerred; 0: DMA

// Create RX and TX Buffer
//
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
	AudioProcessingTask_FreeBuffers();
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
	pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
	if (pRxBuffer == NULL) {
		UARTprintf("Unable to Allocate Memory for Rx Buffer\n\r");
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
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure(I2S_PORT_DMA, 48000);

	g_ucSpkrStartFlag = 1;
// Start Audio Tx/Rx
//
	Audio_Start();

// Start the Microphone Task
//
	Speaker1(file);
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	UARTprintf("g_iReceiveCount %d\n\r", g_iReceiveCount);
	close_codec_NAU();
	UARTprintf("close_codec_NAU");
	Audio_Stop();
	McASPDeInit();
	DestroyCircularBuffer(pRxBuffer);
	UARTprintf("DestroyCircularBuffer(pRxBuffer)");
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	AudioProcessingTask_AllocBuffers();
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	return 0;

}

int Cmd_play_buff(int argc, char *argv[]) {
    int vol = atoi( argv[1] );
    char * file = argv[2];
    return play_ringtone( vol, file );
}
int Cmd_fs_delete(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	int err;

	if (err = sl_FsDel((unsigned char*)argv[1], 0)) {
		UARTprintf("error %d\n", err);
		return -1;
	}

	// Return success.
	return (0);
}


#define YEAR_TO_DAYS(y) ((y)*365 + (y)/4 - (y)/100 + (y)/400)

void untime(unsigned long unixtime, SlDateTime_t *tm)
{
    /* First take out the hour/minutes/seconds - this part is easy. */

    tm->sl_tm_sec = unixtime % 60;
    unixtime /= 60;

    tm->sl_tm_min = unixtime % 60;
    unixtime /= 60;

    tm->sl_tm_hour = unixtime % 24;
    unixtime /= 24;

    /* unixtime is now days since 01/01/1970 UTC
     * Rebaseline to the Common Era */

    unixtime += 719499;

    /* Roll forward looking for the year.  This could be done more efficiently
     * but this will do.  We have to start at 1969 because the year we calculate here
     * runs from March - so January and February 1970 will come out as 1969 here.
     */
    for (tm->sl_tm_year = 1969; unixtime > YEAR_TO_DAYS(tm->sl_tm_year + 1) + 30; tm->sl_tm_year++)
        ;

    /* OK we have our "year", so subtract off the days accounted for by full years. */
    unixtime -= YEAR_TO_DAYS(tm->sl_tm_year);

    /* unixtime is now number of days we are into the year (remembering that March 1
     * is the first day of the "year" still). */

    /* Roll forward looking for the month.  1 = March through to 12 = February. */
    for (tm->sl_tm_mon = 1; tm->sl_tm_mon < 12 && unixtime > 367*(tm->sl_tm_mon+1)/12; tm->sl_tm_mon++)
        ;

    /* Subtract off the days accounted for by full months */
    unixtime -= 367*tm->sl_tm_mon/12;

    /* unixtime is now number of days we are into the month */

    /* Adjust the month/year so that 1 = January, and years start where we
     * usually expect them to. */
    tm->sl_tm_mon += 2;
    if (tm->sl_tm_mon > 12)
    {
        tm->sl_tm_mon -= 12;
        tm->sl_tm_year++;
    }

    tm->sl_tm_day = unixtime;
}

unsigned long get_time() {
	portTickType now = xTaskGetTickCount();
	unsigned long ntp = 0;
	static unsigned long last_ntp = 0;
	unsigned int tries = 0;

	if (last_ntp == 0) {

		while (ntp == 0) {
			while (!(sl_status & HAS_IP)) {
				vTaskDelay(100);
			} //wait for a connection the first time...

			ntp = last_ntp = unix_time();

			vTaskDelay((1 << tries) * 1000);
			if (tries++ > 5) {
				tries = 5;
			}

			if( ntp != 0 ) {
				SlDateTime_t tm;
				untime( ntp, &tm );
				UARTprintf( "setting sl time %d:%d:%d day %d mon %d yr %d", tm.sl_tm_hour,tm.sl_tm_min,tm.sl_tm_sec,tm.sl_tm_day,tm.sl_tm_mon,tm.sl_tm_year);

				sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
						  SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
						  sizeof(SlDateTime_t),(unsigned char *)(&tm));
			}
		}

	} else if (last_ntp != 0) {
        SlDateTime_t dt =  {0};
        _u8 configLen = sizeof(SlDateTime_t);
        _u8 configOpt = SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
        sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&configOpt, &configLen,(_u8 *)(&dt));

        ntp = dt.sl_tm_sec + dt.sl_tm_min*60 + dt.sl_tm_hour*3600 + dt.sl_tm_year_day*86400 +
				(dt.sl_tm_year-70)*31536000 + ((dt.sl_tm_year-69)/4)*86400 -
				((dt.sl_tm_year-1)/100)*86400 + ((dt.sl_tm_year+299)/400)*86400 + 171398145;
	}
	return ntp;
}

static xSemaphoreHandle alarm_smphr;
static SyncResponse_Alarm alarm;
#define ONE_YEAR_IN_SECONDS 0x1E13380

void set_alarm( SyncResponse_Alarm * received_alarm ) {
    if (xSemaphoreTake(alarm_smphr, portMAX_DELAY)) {
        if (received_alarm->has_ring_offset_from_now_in_second && received_alarm->has_ring_duration_in_second ) {
        	unsigned long now = get_time();
        	received_alarm->start_time = now + received_alarm->ring_offset_from_now_in_second;
        	received_alarm->end_time = now + received_alarm->ring_offset_from_now_in_second + received_alarm->ring_duration_in_second;
        	received_alarm->has_end_time = received_alarm->has_start_time = true;
        	//sanity check
            if( received_alarm->start_time - now < ONE_YEAR_IN_SECONDS ) {
                //are we within the duration of the current alarm?
                if( alarm.has_start_time
                 && alarm.start_time - now > 0
                 && now - alarm.start_time < alarm.ring_duration_in_second ) {
                    UARTprintf( "alarm currently active, putting off setting\n");
                } else {
                    memcpy(&alarm, received_alarm, sizeof(alarm));
                }
            } else {
                UARTprintf( "alarm cancelled\n");
                memcpy(&alarm, received_alarm, sizeof(alarm));
            }
            UARTprintf("Got alarm %d to %d in %d minutes\n",
                        received_alarm->start_time, received_alarm->end_time,
                        (received_alarm->start_time - now) / 60);
        }else{
            UARTprintf("No alarm for now.\n");
        }

        xSemaphoreGive(alarm_smphr);
    }
}

void thread_alarm(void * unused) {
	while (1) {
		portTickType now = xTaskGetTickCount();
		//todo audio processing
		uint32_t time = get_time();
		if (xSemaphoreTake(alarm_smphr, portMAX_DELAY)) {

			if(alarm.has_start_time && alarm.start_time > 0)
			{
				if ( time - alarm.start_time < alarm.ring_duration_in_second ) {
					UARTprintf("ALARM RINGING RING RING RING\n");
					xSemaphoreGive(alarm_smphr);
					if (!g_ucSpkrStartFlag) {
						play_ringtone(57, AUDIO_FILE);
					}
					xSemaphoreTake(alarm_smphr, portMAX_DELAY);
				} else if(g_ucSpkrStartFlag) {
					g_ucSpkrStartFlag = 0;
					UARTprintf("ALARM DONE RINGING\n");
					alarm.has_end_time = 0;
					alarm.has_start_time = 0;
				}
			}else{
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

static int light_m2,light_mean, light_cnt,light_log_sum,light_sf;
static xSemaphoreHandle light_smphr;

 xSemaphoreHandle i2c_smphr;
 int Cmd_led(int argc, char *argv[]) ;

void thread_fast_i2c_poll(void * unused)  {
	int last_prox =0;
	portTickType last = 0;
	while (1) {
		portTickType now = xTaskGetTickCount();
		int light;
		int prox,hpf_prox=0;

		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			int adjust;

			vTaskDelay(2);
			light = get_light();
			vTaskDelay(2); //this is important! If we don't do it, then the prox will stretch the clock!

			// For the black morpheus, we can detect 6mm distance max
			// for white one, 9mm distance max.
			prox = get_prox();  // now this thing is in um.

			hpf_prox += ( (last_prox - prox) - hpf_prox )>>2;   // The noise in enclosure is in 100+ um level

			//UARTprintf("PROX: %d um\n", prox);
			if(hpf_prox > 400 && now - last > 2000 ) {  // seems not very sensitive,  the noise in enclosure is in 100+ um level
				UARTprintf("PROX: %d um, diff %d um\n", prox, hpf_prox);
				last = now;
				hpf_prox = 0;
				xSemaphoreTake(alarm_smphr, portMAX_DELAY);
				if (alarm.has_start_time && get_time() >= alarm.start_time) {
					memset(&alarm, 0, sizeof(alarm));
				}
				xSemaphoreGive(alarm_smphr);
				xSemaphoreGive(i2c_smphr);
				g_ucSpkrStartFlag = 0;

				uint8_t adjust_max_light = 80;

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
			} else {
				xSemaphoreGive(i2c_smphr);
			}
			last_prox = prox;

			if (xSemaphoreTake(light_smphr, portMAX_DELAY)) {
				light_log_sum += bitlog(light);
				++light_cnt;

				int delta = light - light_mean;
				light_mean = light_mean + delta/light_cnt;
				light_m2 = light_m2 + delta * ( light - light_mean);
				if( light_m2 < 0 ) {
					light_m2 = 0x7FFFFFFF;
				}

				//UARTprintf( "%d %d %d %d\n", delta, light_mean, light_m2, light_cnt);
				xSemaphoreGive(light_smphr);
			}
		}
		vTaskDelayUntil(&now, 100);
	}
}

xQueueHandle data_queue = 0;

void thread_tx(void* unused) {
	data_t data;

	load_aes();

	while (1) {
		int tries = 0;
		UARTprintf("********************Start polling *****************\n");
		if( data_queue != 0 && !xQueueReceive( data_queue, &( data ), portMAX_DELAY ) ) {
			vTaskDelay(100);
			UARTprintf("*********************** Waiting for data *****************\n");
			continue;
		}

		UARTprintf("sending time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d\n",
				data.time, data.light, data.light_variability, data.light_tonality, data.temp, data.humid,
				data.dust);
		data.pill_list = pill_list;

		while (!send_periodic_data(&data) == 0) {
			UARTprintf("********************* Waiting for WIFI connection *****************\n");
			vTaskDelay( (1<<tries) * 1000 );
			if( tries++ > 5 ) {
				tries = 5;
			}
			do {vTaskDelay(1000);} //wait for a connection...
			while( !(sl_status&HAS_IP ) );
		}//try every little bit
	}
}

xSemaphoreHandle pill_smphr;

void thread_sensor_poll(void* unused) {

	//
	// Print some header text.
	//

	data_t data;

	while (1) {
		portTickType now = xTaskGetTickCount();

		data.time = get_time();

		if( xSemaphoreTake( dust_smphr, portMAX_DELAY ) ) {
			int dust_var;

			if( dust_cnt != 0 ) {
				data.dust = dust_mean;
			} else {
				data.dust = get_dust();
			}
			dust_log_sum /= dust_cnt;

			dust_var = dust_m2 / (dust_cnt-1);

			data.dust_var = dust_var;
			data.dust_max = dust_max;
			data.dust_min = dust_min;

			dust_min = 5000;
			dust_m2 = dust_mean = dust_cnt = dust_log_sum = dust_max = 0;

			xSemaphoreGive( dust_smphr );
		} else {
			data.dust = get_dust();
		}
		if (xSemaphoreTake(light_smphr, portMAX_DELAY)) {
			light_log_sum /= light_cnt;
			light_sf = (light_mean<<8) / bitexp( light_log_sum );

			int light_var = light_m2 / (light_cnt-1);

			//UARTprintf( "%d lightsf %d var %d cnt\n", light_sf, light_var, light_cnt );

			data.light_variability = light_var;
			data.light_tonality = light_sf;
			data.light = light_mean;
			light_m2 = light_mean = light_cnt = light_log_sum = light_sf = 0;
			xSemaphoreGive(light_smphr);
		}
		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			uint8_t measure_time = 10;
			int64_t humid_sum = 0;
			int64_t temp_sum = 0;
			while(--measure_time)
			{
				vTaskDelay(2);
				humid_sum += get_humid();
				vTaskDelay(2);
				temp_sum += get_temp();
				vTaskDelay(2);
			}

			data.humid = humid_sum / 10;
			data.temp = temp_sum / 10;
			
			xSemaphoreGive(i2c_smphr);
		} else {
			continue;
		}
		if (xSemaphoreTake(pill_smphr, portMAX_DELAY)) {
			data.pill_list = pill_list;
			xSemaphoreGive(pill_smphr);
		} else {
			continue;
		}
		UARTprintf("collecting time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d %d %d %d\n",
				data.time, data.light, data.light_variability, data.light_tonality, data.temp, data.humid,
				data.dust, data.dust_max, data.dust_min, data.dust_var);

		    // ...

        xQueueSend( data_queue, ( void * ) &data, 10 );

		UARTprintf("delay...\n");

		vTaskDelayUntil(&now, 60 * configTICK_RATE_HZ);
	}

}

int Cmd_test_network(int argc,char * argv[]) {
	TestNetwork_RunTests(TEST_SERVER);

	return 0;
}

// ==============================================================================
// This function implements the "tasks" command.  It prints a list of the
// current FreeRTOS tasks with information about status, stack usage, etc.
// ==============================================================================
#if ( configUSE_TRACE_FACILITY == 1 )

int Cmd_tasks(int argc, char *argv[]) {
	char* pBuffer;

	pBuffer = pvPortMalloc(1024);
	assert(pBuffer);
	vTaskList(pBuffer);
	UARTprintf("\t\t\t\t\tUnused\nTaskName\t\tStatus\tPri\tStack\tTask ID\n");
	UARTprintf("=======================================================");
	UARTprintf("%s", pBuffer);

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
	UARTprintf("\nAvailable commands\n");
	UARTprintf("------------------\n");

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
		UARTprintf("%s: %s\n", pEntry->pcCmd, pEntry->pcHelp);

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

    UARTprintf( "SSID RSSI\n" );
	for(i=0;i<lCountSSID;++i) {
		UARTprintf( "%s %d\n", g_netEntries[i].ssid, g_netEntries[i].rssi );
	}
	return 0;
}



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
		UARTprintf("nordic interrupt\r\n");
#endif
		xSemaphoreGiveFromISR(spi_smphr, &xHigherPriorityTaskWoken);
		MAP_GPIOIntDisable(GPIO_PORT,GSPI_INT_PIN);
	    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
	if (status & RTC_INT_PIN) {
		UARTprintf("prox interrupt\r\n");
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

xSemaphoreHandle pill_smphr;

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
		UARTprintf("Decoded: %s \r\n", hci_decode(message, len, NULL));
		hci_free(message);
	}else{
		uint8_t * message = hci_encode("hello", strlen("hello") + 1, &len);
		UARTprintf("Decoded: %s \r\n", hci_decode(message, len, NULL));
		hci_free(message);
	}
	return 0;
}

int Cmd_topdfu(int argc, char *argv[]){
	if(argc > 1){
		return top_board_dfu_begin(argv[1]);
	}
	UARTprintf("Usage: topdfu $full_path_to_file");
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
		{ "free", Cmd_free, "Report free memory" },
		{ "connect", Cmd_connect, "Connect to an AP" },
		{ "disconnect", Cmd_disconnect, "disconnect to an AP" },
		{ "mac", Cmd_set_mac, "set the mac" },
		{ "aes", Cmd_set_aes, "set the aes key" },

		{ "ping", Cmd_ping, "Ping a server" },
		{ "time", Cmd_time, "get ntp time" },
		{ "status", Cmd_status, "status of simple link" },
		{ "audio", Cmd_audio_test, "audio upload test" },

    { "mnt",      Cmd_mnt,      "Mount the SD card" },
    { "umnt",     Cmd_umnt,     "Unount the SD card" },
    { "ls",       Cmd_ls,       "Display list of files" },
    { "chdir",    Cmd_cd,       "Change directory" },
    { "cd",       Cmd_cd,       "alias for chdir" },
    { "mkdir",    Cmd_mkdir,    "make a directory" },
    { "rm",       Cmd_rm,       "Remove file" },
    { "write",    Cmd_write,    "Write some text to a file" },
    { "write_file",    Cmd_write_file,    "Write some text to a file" },
    { "mkfs",     Cmd_mkfs,     "Make filesystem" },
    { "pwd",      Cmd_pwd,      "Show current working directory" },
    { "cat",      Cmd_cat,      "Show contents of a text file" },
		{ "fault", Cmd_fault, "Trigger a hard fault" },
		{ "i2crd", Cmd_i2c_read,"i2c read" },
		{ "i2cwr", Cmd_i2c_write, "i2c write" },
		{ "i2crdrg", Cmd_i2c_readreg, "i2c readreg" },
        { "i2cwrrg", Cmd_i2c_writereg, "i2c_writereg" },

		{ "humid", Cmd_readhumid, "i2 read humid" },
		{ "temp", Cmd_readtemp,	"i2 read temp" },
		{ "light", Cmd_readlight, "i2 read light" },
		{"proximity", Cmd_readproximity, "i2 read proximity" },
		{"codec_Mic", get_codec_mic_NAU, "i2s mic_codec" },
		{"auto_saveSD", Cmd_write_record, "automatic save data into SD"},
		{"append", Cmd_append,"Cmd_test_append_content"},
#if ( configUSE_TRACE_FACILITY == 1 )
		{ "tasks", Cmd_tasks, "Report stats of all tasks" },
#endif

		{ "dust", Cmd_dusttest, "dust sensor test" },


		{ "fswr", Cmd_fs_write, "fs write" },
		{ "fsrd", Cmd_fs_read, "fs read" },
		{ "play_ringtone", Cmd_code_playbuff, "play selected ringtone" },
		{ "stop_ringtone", Audio_Stop,"stop sounds"},
		{ "r", Cmd_record_buff,"record sounds into SD card"},
		{ "p", Cmd_play_buff, "play sounds from SD card"},
		{ "aud",Cmd_audio_do_stuff,"command the audio on/off"},
		{ "fsdl", Cmd_fs_delete, "fs delete" },
		//{ "readout", Cmd_readout_data, "read out sensor data log" },

		{ "sl", Cmd_sl, "start smart config" },
		{ "mode", Cmd_mode, "set the ap/station mode" },

		{ "spird", Cmd_spi_read,"spi read" },
		{ "spiwr", Cmd_spi_write, "spi write" },
		{ "spirst", Cmd_spi_reset, "spi reset" },

		{ "antsel", Cmd_antsel, "select antenna" },
		{ "led", Cmd_led, "led test pattern" },
		{ "clrled", Cmd_led_clr, "led test pattern" },

		{ "rdiostats", Cmd_RadioGetStats, "radio stats" },
		{ "rdiotxstart", Cmd_RadioStartTX, "start tx test" },
		{ "rdiotxstop", Cmd_RadioStopTX, "stop tx test" },
		{ "rdiorxstart", Cmd_RadioStartRX, "start rx test" },
		{ "rdiorxstop", Cmd_RadioStopRX, "stop rx test" },
		{ "rssi", Cmd_rssi, "scan rssi" },
		{ "slip", Cmd_slip, "slip test" },
		{ "data_upload", Cmd_data_upload, "upload protobuf data" },
		{ "^", Cmd_send_top, "send command to top board"},
		{ "topdfu", Cmd_topdfu, "update topboard firmware."},
		{ "factory_reset", Cmd_factory_reset, "Factory reset from middle."},
		{ "download", Cmd_download, "download test function."},
		{ "dtm", Cmd_top_dtm, "Sends Direct Test Mode command" },
		{ "animate", Cmd_led_animate, "Animates led"},
		{"test_network",Cmd_test_network,"tests network task, usage: test_network ip.of.host.server"},

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

void vUARTTask(void *pvParameters) {
	char cCmdBuf[512];
	portTickType now;
	NetworkTaskData_t network_task_data;

	memset(&network_task_data,0,sizeof(network_task_data));

	if(led_init() != 0){
		UARTprintf("Failed to create the led_events.\n");
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

	UARTprintf("Boot\n");

	//default to IFA
	antsel(IFA_ANT);

	UARTprintf("*");
	now = xTaskGetTickCount();
	sl_mode = sl_Start(NULL, NULL, NULL);
	UARTprintf("*");
	while (sl_mode != ROLE_STA) {
		UARTprintf("+");
		sl_WlanSetMode(ROLE_STA);
		sl_Stop(1);
		sl_mode = sl_Start(NULL, NULL, NULL);
	}
	UARTprintf("*");

	// Set connection policy to Auto
	sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 0), NULL, 0);

	UARTprintf("*");
	unsigned char mac[6];
	unsigned char mac_len;
	sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
	UARTprintf("*");

	// SDCARD INITIALIZATION
	// Enable MMCHS, Reset MMCHS, Configure MMCHS, Configure card clock, mount
	MAP_PRCMPeripheralClkEnable(PRCM_SDHOST, PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralReset(PRCM_SDHOST);
	MAP_SDHostInit(SDHOST_BASE);
	MAP_SDHostSetExpClk(SDHOST_BASE, MAP_PRCMPeripheralClockGet(PRCM_SDHOST),
			1000000);
	UARTprintf("*");
	Cmd_mnt(0, 0);
	UARTprintf("*");
	//INIT SPI
	spi_init();
	UARTprintf("*");

	vTaskDelayUntil(&now, 1000);
	UARTprintf("*");

	if (sl_mode == ROLE_AP || !sl_status) {
		//Cmd_sl(0, 0);
	}

	// Init sensors
	init_humid_sensor();
	init_temp_sensor();
	init_light_sensor();
	init_prox_sensor();

	data_queue = xQueueCreate(60, sizeof(data_t));
	vSemaphoreCreateBinary(dust_smphr);
	vSemaphoreCreateBinary(light_smphr);
	vSemaphoreCreateBinary(i2c_smphr);
	vSemaphoreCreateBinary(spi_smphr);
	vSemaphoreCreateBinary(pill_smphr);
	vSemaphoreCreateBinary(alarm_smphr);


	if (data_queue == 0) {
		UARTprintf("Failed to create the data_queue.\n");
	}

	xTaskCreate(top_board_task, "top_board_task", 1024 / 4, NULL, 2, NULL); //todo reduce stack
	xTaskCreate(thread_alarm, "alarmTask", 2 * 1024 / 4, NULL, 4, NULL); //todo reduce stack

	UARTprintf("*");
	xTaskCreate(thread_spi, "spiTask", 2*1024 / 4, NULL, 5, NULL);

	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(NetworkTask_Thread,"networkTask",4*1024/4,&network_task_data,10,NULL);

	SetupGPIOInterrupts();
	UARTprintf("*");
#if !ONLY_MID
	xTaskCreate(AudioCaptureTask_Thread,"audioCaptureTask",4*1024/4,NULL,4,NULL);
	UARTprintf("*");
	xTaskCreate(AudioProcessingTask_Thread,"audioProcessingTask",1*1024/4,NULL,1,NULL);
	UARTprintf("*");
	xTaskCreate(thread_fast_i2c_poll, "fastI2CPollTask",  1024 / 4, NULL, 13, NULL);
	UARTprintf("*");
	xTaskCreate(thread_dust, "dustTask", 256 / 4, NULL, 3, NULL);
	UARTprintf("*");
	xTaskCreate(thread_sensor_poll, "pollTask", 1024 / 4, NULL, 4, NULL);
	UARTprintf("*");
	xTaskCreate(thread_tx, "txTask", 3 * 1024 / 4, NULL, 2, NULL);
	UARTprintf("*");
	xTaskCreate(thread_ota, "otaTask",5 * 1024 / 4, NULL, 1, NULL);
	UARTprintf("*");
#endif
	//checkFaults();



	UARTprintf("\n\nFreeRTOS %s, %d, %s %x%x%x%x%x%x\n",
	tskKERNEL_VERSION_NUMBER, KIT_VER, MORPH_NAME, mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
	UARTprintf("\n? for help\n");
	UARTprintf("> ");

	/* remove anything we recieved before we were ready */

	/* Loop forever */
	while (1) {
		/* Wait for a signal indicating we have an RX line to process */
		xSemaphoreTake(g_xRxLineSemaphore, portMAX_DELAY);

		if (UARTPeek('\r') != -1) {
			/* Read data from the UART and process the command line */
			UARTgets(cCmdBuf, sizeof(cCmdBuf));
			if (ustrlen(cCmdBuf) == 0) {
				UARTprintf("> ");
				continue;
			}

			//
			// Pass the line from the user to the command processor.  It will be
			// parsed and valid commands executed.
			//
			char * args = pvPortMalloc( sizeof(cCmdBuf) );
			memcpy( args, cCmdBuf, sizeof( cCmdBuf ) );
			xTaskCreate(CmdLineProcess, "commandTask",  5*1024 / 4, args, 20, NULL);
        }
	}
}
