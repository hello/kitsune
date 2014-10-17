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

#include "fault.h"

#include "cmdline.h"
#include "ustdlib.h"
#include "uartstdio.h"

#include "wifi_cmd.h"
#include "i2c_cmd.h"
#include "dust_cmd.h"
#include "fatfs_cmd.h"
#include "spi_cmd.h"
#include "audiofeatures.h"

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

#include "slip_packet.h"
//#include "mcasp_if.h" // add by Ben

#define ONLY_MID 0

#define NUM_LOGS 72
#if 0
//*****************************************************************************
//
// Define Packet Size, Rx and Tx Buffer
//
//*****************************************************************************
#define PACKET_SIZE             100
#define PLAY_WATERMARK		30*256
#define TX_BUFFER_SIZE          10*PACKET_SIZE
#define RX_BUFFER_SIZE          10*PACKET_SIZE
#endif

extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;
tUDPSocket g_UdpSock;

OsiTaskHandle g_SpeakerTask = NULL ;
OsiTaskHandle g_MicTask = NULL ;
//******************************************************************************
//			        FUNCTION DECLARATIONS
//******************************************************************************
//extern void Speaker( void *pvParameters );
extern void Microphone( void *pvParameters );
extern void Speaker( void *pvParameters );
extern int g_iSentCount;
extern int g_iReceiveCount;
unsigned long tone;
//*****************************************************************************
//                      GLOBAL VARIABLES
//*****************************************************************************
P_AUDIO_HANDLER g_pAudioInControlHdl;
P_AUDIO_HANDLER g_pAudioOutControlHdl;
//******************************************************************************
//			    GLOBAL VARIABLES
//******************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

unsigned int clientIP;
tCircularBuffer *pTxBuffer;
tCircularBuffer *pRxBuffer;
//tUDPSocket g_UdpSock;
unsigned long g_ulMcASPStatus = 0;
unsigned long g_ulRxCounter = 0;
unsigned long g_ulTxCounter = 0;
unsigned long g_ulZeroCounter = 0;
unsigned long g_ulValue = 0;
int iCounter,i = 0;
//extern unsigned char iDone;
//OsiTaskHandle g_SpeakerTask = NULL ;
//OsiTaskHandle g_MicTask = NULL ;
//static FIL file_obj;
//const char* file_name = "/POD2";
//FRESULT res;
//FILINFO file_info;
//*****************************************************************************
//                          LOCAL DEFINES
//*****************************************************************************
#define OSI_STACK_SIZE          256

//unsigned char speaker_data[16*1024];
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

	if (bytes = sl_FsRead(hndl, 0, (unsigned char*)buffer, minval(info.FileLen, BUF_SZ))) {
		UARTprintf("read %d bytes\n", bytes);
	}

	sl_FsClose(hndl, 0, 0, 0);

	for (i = 0; i < bytes; ++i) {
		UARTprintf("%x", buffer[i]);
	}

	// Return success.
	return (0);
}
extern
unsigned short * audio_buf;
int Cmd_code_playbuff(int argc, char *argv[]) {
unsigned int CPU_XDATA = 1; //1: enabled CPU interrupt triggerred
#define minval( a,b ) a < b ? a : b
	unsigned long tok;
	long hndl, err, bytes;

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

	get_codec_NAU();
	UARTprintf(" Done for get_codec_NAU\n ");

	AudioCaptureRendererConfigure(I2S_PORT_CPU, 48000);

	AudioCapturerInit(CPU_XDATA, 48000); //UARTprintf(" Done for AudioCapturerInit\n ");

	Audio_Start(); //UARTprintf(" Done for Audio_Start\n ");

	vPortFree(audio_buf); //UARTprintf(" audio_buf\n ");
		// Audio_Stop(); // added this, the ringtone will not play
	 return 0;
}

//extern
void Microphone1();

int Cmd_record_buff(int argc, char *argv[]) {
	unsigned int CPU_XDATA = 0; //1: enabled CPU interrupt triggerred
// Create RX and TX Buffer
//
pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
if(pTxBuffer == NULL)
{
	UARTprintf("Unable to Allocate Memory for Tx Buffer\n\r");
    while(1){};
}
// Configure Audio Codec
//
get_codec_mic_NAU();

// Initialize the Audio(I2S) Module
//
AudioCapturerInit(CPU_XDATA, 48000);

// Initialize the DMA Module
//
UDMAInit();
UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);

//
// Setup the DMA Mode
//
SetupPingPongDMATransferTx();
// Setup the Audio In/Out
//

AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
AudioCaptureRendererConfigure(I2S_PORT_DMA, 48000);

// Start Audio Tx/Rx
//
Audio_Start();


// Start the Microphone Task
//
Microphone1();

UARTprintf("g_iSentCount %d\n\r", g_iSentCount);
Audio_Stop();

DestroyCircularBuffer(pTxBuffer);

return 0;

}

void Speaker1();

int Cmd_play_buff(int argc, char *argv[]) {
	unsigned int CPU_XDATA = 0; //1: enabled CPU interrupt triggerred; 0: DMA
// Create RX and TX Buffer
//
	pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
if(pRxBuffer == NULL)
{
	UARTprintf("Unable to Allocate Memory for Rx Buffer\n\r");
    while(1){};
}
// Configure Audio Codec
//
get_codec_NAU();

// Initialize the Audio(I2S) Module
//
AudioCapturerInit(CPU_XDATA, 22050);

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

AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
AudioCaptureRendererConfigure(I2S_PORT_DMA, 22050);

// Start Audio Tx/Rx
//
Audio_Start();


// Start the Microphone Task
//
Speaker1();

UARTprintf("g_iReceiveCount %d\n\r", g_iReceiveCount);
Audio_Stop();

DestroyCircularBuffer(pRxBuffer); UARTprintf("DestroyCircularBuffer(pRxBuffer)" );

return 0;

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
	static portTickType unix_now = 0;
	unsigned long ntp = 0;
	static unsigned long last_ntp = 0;

	if (last_ntp == 0) {

		while (ntp == 0) {
			while (!(sl_status & HAS_IP)) {
				vTaskDelay(100);
			} //wait for a connection the first time...

			ntp = last_ntp = unix_time();

			if( ntp != 0 ) {
				SlDateTime_t tm;
				untime( ntp, &tm );
				UARTprintf( "setting sl time %d:%d:%d day %d mon %d yr %d", tm.sl_tm_hour,tm.sl_tm_min,tm.sl_tm_sec,tm.sl_tm_day,tm.sl_tm_mon,tm.sl_tm_year);

				sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
						  SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
						  sizeof(SlDateTime_t),(unsigned char *)(&tm));
			}
		}

		unix_now = now;
	} else if (last_ntp != 0) {
		ntp = last_ntp + (now - unix_now) / 1000;
		last_ntp = ntp;
		unix_now = now;
	}
	return ntp;
}

void thread_audio(void * unused) {
	int c=1;
	while (c) {
		portTickType now = xTaskGetTickCount();
		//todo audio processing
		vTaskDelayUntil(&now, 100 ); //todo 10hz - this may need adjusted
	}
}

#define SENSOR_RATE 60

static unsigned int dust_val=0;
static unsigned int dust_cnt=0;
xSemaphoreHandle dust_smphr;

void thread_dust(void * unused)  {
    #define maxval( a, b ) a>b ? a : b
	while (1) {
		if (xSemaphoreTake(dust_smphr, portMAX_DELAY)) {
			++dust_cnt;
			dust_val += get_dust();
			xSemaphoreGive(dust_smphr);
		}

		vTaskDelay( 100 );
	}
}

static int light_m2,light_mean, light_cnt,light_log_sum,light_sf;
static xSemaphoreHandle light_smphr;

 xSemaphoreHandle i2c_smphr;

void thread_fast_i2c_poll(void * unused)  {
	int last_prox =0;
	while (1) {
		portTickType now = xTaskGetTickCount();
		int light;
		int prox,hpf_prox;

		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			vTaskDelay(2);
			light = get_light();
			vTaskDelay(2); //this is important! If we don't do it, then the prox will stretch the clock!
			prox = 0;//todo get_prox();
			hpf_prox = last_prox - prox;
			if (abs(hpf_prox) > 30) {
				UARTprintf("PROX: %d\n", hpf_prox);
			}
			last_prox = prox;

			xSemaphoreGive(i2c_smphr);

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
		vTaskDelayUntil(&now, 100 );
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

		if( alarm.has_start_time && data.time > alarm.start_time ) {
			UARTprintf("ALARM RINGING RING RING RING\n");
			if( alarm.has_end_time && data.time > alarm.end_time ) {
				UARTprintf("ALARM DONE RINGING\n");
				alarm.has_start_time = 0;
			} else {
				alarm.has_start_time = 0;
			}
		}

		if( xSemaphoreTake( dust_smphr, portMAX_DELAY ) ) {
			if( dust_cnt != 0 ) {
				data.dust = dust_val / dust_cnt;
			} else {
				data.dust = get_dust();
			}
			dust_val = dust_cnt = 0;
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
			vTaskDelay(2);
			data.humid = get_humid();
			vTaskDelay(2);
			data.temp = get_temp();
			vTaskDelay(2);
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
		UARTprintf("collecting time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d\n",
				data.time, data.light, data.light_variability, data.light_tonality, data.temp, data.humid,
				data.dust);

		    // ...

        xQueueSend( data_queue, ( void * ) &data, 10 );

		UARTprintf("delay...\n");

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

static void AudioFeatCallback(const int16_t * mfccfeats, const Segment_t * pSegment) {
	int32_t t1;
	int32_t t2;

	t1 = pSegment->t1;
	t2 = pSegment->t2;

	UARTprintf("ACTUAL: t1=%d,t2=%d,energy=%d\n",t1,t2,mfccfeats[0]);
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
static int GetScanResult(Sl_WlanNetworkEntry_t* netEntries )
{
    unsigned char   policyOpt;
    unsigned long IntervalVal = 60;
    int lRetVal;

    policyOpt = SL_CONNECTION_POLICY(0, 0, 0, 0, 0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION , policyOpt, NULL, 0);


    // enable scan
    policyOpt = SL_SCAN_POLICY(1);

    // set scan policy - this starts the scan
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt,
                            (unsigned char *)(IntervalVal), sizeof(IntervalVal));


    // delay 1 second to verify scan is started
    vTaskDelay(1000);

    // lRetVal indicates the valid number of entries
    // The scan results are occupied in netEntries[]
    lRetVal = sl_WlanGetNetworkList(0, SCAN_TABLE_SIZE, netEntries);

    // Disable scan
    policyOpt = SL_SCAN_POLICY(0);

    // set scan policy - this stops the scan
    sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt,
                            (unsigned char *)(IntervalVal), sizeof(IntervalVal));

    return lRetVal;

}
int Cmd_rssi(int argc, char *argv[]) {
	int lCountSSID,i;

	Sl_WlanNetworkEntry_t g_netEntries[SCAN_TABLE_SIZE];

	lCountSSID = GetScanResult(&g_netEntries[0]);

    SortByRSSI(&g_netEntries[0],(unsigned char)lCountSSID);

    UARTprintf( "SSID RSSI\n" );
	for(i=0;i<lCountSSID;++i) {
		UARTprintf( "%s %d\n", g_netEntries[i].ssid, g_netEntries[i].rssi );
	}
	return 0;
}

int Cmd_mel(int argc, char *argv[]) {

    int i,ichunk;
	int16_t x[1024];


	srand(0);

	UARTprintf("EXPECT: t1=%d,t2=%d,energy=something not zero\n",43,86);


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
		UARTprintf("nordic interrupt\r\n");
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
	while(1) {
		if (xSemaphoreTake(spi_smphr, 10000) ) {
			vTaskDelay(10);
			Cmd_spi_read(0, 0);
			MAP_GPIOIntEnable(GPIO_PORT,GSPI_INT_PIN);
		} else {
			MAP_GPIOIntEnable(GPIO_PORT,GSPI_INT_PIN);
		}
	}
}

#define NUM_LED 12
#define LED_GPIO_BIT 0x1
#define LED_GPIO_BASE GPIOA3_BASE

#if defined(ccs)

#endif

#define LED_LOGIC_HIGH_FAST 0
#define LED_LOGIC_LOW_FAST LED_GPIO_BIT
#define LED_LOGIC_HIGH_SLOW LED_GPIO_BIT
#define LED_LOGIC_LOW_SLOW 0

void led_fast( unsigned int* color ) {
	int i;
	unsigned int * end = color + NUM_LED;

	for( ;; ) {
		for (i = 0; i < 24; ++i) {
			if ((*color << i) & 0x800000 ) {
				//1
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_FAST);
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_FAST);
				if( i!=23 ) {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");

				} else {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				}
			} else {
				//0
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_FAST);
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_FAST);
				if( i!=23 ) {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");

				} else {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				}
			}
		}
		if( ++color == end ) {
			break;
		}
	}
}
void led_slow(unsigned int* color) {
	int i;
	unsigned int * end = color + NUM_LED;
	for (;;) {
		for (i = 0; i < 24; ++i) {
			if ((*color << i) & 0x800000) {
				//1
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_SLOW);
				UtilsDelay(5);
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_SLOW);
				if (i != 23) {
					UtilsDelay(5);
				} else {
					UtilsDelay(4);
				}

			} else {
				//0
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_SLOW);
				UtilsDelay(1);
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_SLOW);
				if (i != 23) {
					UtilsDelay(5);
				} else {
					UtilsDelay(2);
				}
			}
		}
		if (++color == end) {
			break;
		}
	}
}

#define LED_GPIO_BASE_DOUT GPIOA2_BASE
#define LED_GPIO_BIT_DOUT 0x80
void led_array(unsigned int * colors) {
	int i;
	unsigned long ulInt;
	//

	// Temporarily turn off interrupts.
	//
	bool fast = MAP_GPIOPinRead(LED_GPIO_BASE_DOUT, LED_GPIO_BIT_DOUT);
	ulInt = MAP_IntMasterDisable();
	if (fast) {
		for (i = 0; i < NUM_LED; ++i) {
			led_fast(colors + i);
		}
	} else {
		for (i = 0; i < NUM_LED; ++i) {
			led_slow(colors + i);
		}
	}
	if (!ulInt) {
		MAP_IntMasterEnable();
	}
}
void led_ccw( unsigned int * colors) {
	int l;
	for (l = 0; l < NUM_LED-2; ++l) {
		int temp = colors[l];
		colors[l] = colors[l + 1];
		colors[l + 1] = temp;
	}
}
void led_cw( unsigned int * colors) {
	int l;
	for (l = NUM_LED-2; l > -1; --l) {
		int temp = colors[l];
		colors[l] = colors[l + 1];
		colors[l + 1] = temp;
	}
}
void led_brightness(unsigned int * colors, unsigned int brightness ) {
	int l;
	unsigned int blue,red,green;

	for (l = 0; l < NUM_LED; ++l) {
		blue = ( colors[l] & ~0xffff00 );
		red = ( colors[l] & ~0xff00ff );
		green = ( colors[l] & ~0x00ffff );

		blue = (brightness * blue)>>8;
		red = (brightness * red)>>8;
		green = (brightness * green)>>8;
		colors[l] = (blue) | (red<<8) | (green<<16);
	}
}

int Cmd_led(int argc, char *argv[]) {
	int i;
	unsigned int colors[NUM_LED]= {2,4,8,16,32,64,128,255,0,0,0,0};
	unsigned int colors_o[NUM_LED]= {2,4,8,16,32,64,128,255,0,0,0,0};

	//colors[0] = atoi(argv[1]);
	for (i = 1; i < 32; ++i) {
		led_cw(colors_o);
		memcpy( colors, colors_o, sizeof(colors));
		led_brightness( colors, fxd_sin(i<<4)>>7);
		led_array(colors);
		vTaskDelay(8*(12-(fxd_sin((i+1)<<4)>>12)));
	}
	vTaskDelay(1);
	memset(colors, 0, sizeof(colors));
	led_array(colors);
	return 0;
}

int Cmd_led_clr(int argc, char *argv[]) {
	unsigned int colors[NUM_LED] = { 0 };
	led_array(colors);

	return 0;
}

int Cmd_slip(int argc, char * argv[]){
	uint8_t test_packet[] = {0x2, 0x00, 0x00, 0x00, 0xB8, 0x43, 0x00, 0x00};
	slip_reset();
	uint8_t * ret = (uint8_t*)slip_write(test_packet, sizeof(test_packet));
	if(ret){
		int i;
		UARTprintf("\r\nSLIP TEST");
		for(i = 0; i < (sizeof(test_packet) + 8); i++){
			UARTprintf("0x%02X ",*(uint8_t*)(ret+i));
		}
		UARTprintf("\r\n");
		slip_free(ret);
	}
	return 0;
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
		{"codec_NAU8814", get_codec_NAU, "i2 nuvoton_codec" },
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
		{ "fsdl", Cmd_fs_delete, "fs delete" },
		//{ "readout", Cmd_readout_data, "read out sensor data log" },

		{ "sl", Cmd_sl, "start smart config" },
		{ "mode", Cmd_mode, "set the ap/station mode" },
		{ "mel", Cmd_mel, "test the mel calculation" },

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
	char cCmdBuf[64];
	portTickType now;

	Cmd_led_clr(0,0);

	UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);

	UARTprintf("Booting...\n");

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

	// Set connection policy to Auto + SmartConfig
	//      (Device's default connection policy)
	sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1),
			NULL, 0);

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

	data_queue = xQueueCreate(60, sizeof(data_t));
	vSemaphoreCreateBinary(dust_smphr);
	vSemaphoreCreateBinary(light_smphr);
	vSemaphoreCreateBinary(i2c_smphr);
	vSemaphoreCreateBinary(spi_smphr);
	vSemaphoreCreateBinary(pill_smphr);

	if (data_queue == 0) {
		UARTprintf("Failed to create the data_queue.\n");
	}

	xTaskCreate(thread_audio, "audioTask", 5 * 1024 / 4, NULL, 4, NULL); //todo reduce stack
	UARTprintf("*");
	xTaskCreate(thread_spi, "spiTask", 5*2048 / 4, NULL, 5, NULL);
	SetupGPIOInterrupts();
	UARTprintf("*");
#if !ONLY_MID
	xTaskCreate(thread_fast_i2c_poll, "fastI2CPollTask", 5 * 1024 / 4, NULL, 3, NULL);
	UARTprintf("*");
	xTaskCreate(thread_dust, "dustTask", 5* 1024 / 4, NULL, 3, NULL);
	UARTprintf("*");
	xTaskCreate(thread_sensor_poll, "pollTask", 5 * 1024 / 4, NULL, 4, NULL);
	UARTprintf("*");
	xTaskCreate(thread_tx, "txTask", 5 * 1024 / 4, NULL, 2, NULL);
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
	UARTFlushRx();

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
			xTaskCreate(CmdLineProcess, "commandTask",  2*1024 / 4, cCmdBuf, 9, NULL);
		}
	}
}
