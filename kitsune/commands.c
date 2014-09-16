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
#include "sdhost.h"
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
//#include "mcasp_if.h" // add by Ben

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
extern void Speaker( void *pvParameters );
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
	unsigned long tok;
	int err;
	long hndl, bytes;
	SlFsFileInfo_t info;

	sl_FsGetInfo(argv[1], tok, &info);

	if (sl_FsOpen(argv[1],
	FS_MODE_OPEN_WRITE, &tok, &hndl)) {
		UARTprintf("error opening file, trying to create\n");

		if (sl_FsOpen(argv[1],
				FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok,
				&hndl)) {
			UARTprintf("error opening for write\n");
			return -1;
		}
	}

	bytes = sl_FsWrite(hndl, info.FileLen, argv[2], strlen(argv[2]));
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
	unsigned long tok;
	long hndl, err, bytes, i;
	SlFsFileInfo_t info;
	char buffer[BUF_SZ];

	sl_FsGetInfo(argv[1], tok, &info);

	if (err = sl_FsOpen(argv[1], FS_MODE_OPEN_READ, &tok, &hndl)) {
		UARTprintf("error opening for read %d\n", err);
		return -1;
	}

	if (bytes = sl_FsRead(hndl, 0, buffer, minval(info.FileLen, BUF_SZ))) {
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

#define minval( a,b ) a < b ? a : b
	unsigned long tok;
	long hndl, err, bytes, i,j;
	SlFsFileInfo_t info;

	audio_buf = (char*)pvPortMalloc(AUDIO_BUF_SZ);

	if (err = sl_FsOpen("Ringtone_hello_leftchannel_16PCM", FS_MODE_OPEN_READ, &tok, &hndl)) {
		UARTprintf("error opening for read %d\n", err);
		return -1;
	}
	if (bytes = sl_FsRead(hndl, 0, audio_buf, AUDIO_BUF_SZ)) {
		UARTprintf("read %d bytes\n", bytes);
	}
	sl_FsClose(hndl, 0, 0, 0);
	// Create RX and TX Buffer
    //
   // pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
	//UARTprintf("Done for CreateCircularBuffer TX\n ");
    //pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
	//UARTprintf("Done for CreateCircularBuffer RX\n ");
# if 0
	for (i = 0; i < bytes; ++i) {
		UARTprintf("%x", buffer[i]);
		//buffer[i] = CreateCircularBuffer(TX_BUFFER_SIZE);
		//pTxBuffer->pucWritePtr = buffer[i+1]<<8 + buffer[i];
		//speaker_data = buffer[i];
		//UARTprintf("%x\d\n\r", pRxBuffer->pucReadPtr);
	    //unsigned char *pucReadPtr;
	    //unsigned char *pucWritePtr;
	    //unsigned char *pucBufferStartPtr;
	    //unsigned long ulBufferSize;
	    //unsigned char *pucBufferEndPtr;
		//UARTprintf("%x\n", pRxBuffer->pucWritePtr);
	    // put data in the buffer
	}
# endif
	get_codec_NAU();
	UARTprintf(" Done for get_codec_NAU\n ");
	//UARTprintf(" Done for ControlTaskCreate\n ");

	    // Initialize the DMA Module
	    //
/*
	    UDMAInit(); UARTprintf(" Done for UDMAInit\n ");
	    UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL); UARTprintf(" Done for UDMA_CH4_I2S_RX\n ");
	    UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL); UARTprintf(" Done for UDMA_CH5_I2S_TX\n ");
	    //
	    // Setup the DMA Mode
	    //
	    SetupPingPongDMATransferTx(); UARTprintf(" Done for SetupPingPongDMATransferTx\n ");
	    SetupPingPongDMATransferRx(); UARTprintf(" Done for SetupPingPongDMATransferRx\n ");
	    //
	    // Setup the Audio In/Out
	    //
	    AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	    UARTprintf(" Done for AudioCapturerSetupDMAMode\n ");
*/
	    AudioCaptureRendererConfigure();
	    //UARTprintf(" Done for AudioCaptureRendererConfigure\n ");

//#	if 0
		 //I2SDataPutNonBlocking(I2S_BASE,I2S_DATA_LINE_0,sin[i%16]);
		 //McASPLoad(tmp, BUF_SZ/sizeof(unsigned long));

//		 unsigned short *tmp = (unsigned short*)buffer;
		 AudioCapturerInit(); //UARTprintf(" Done for AudioCapturerInit\n ");
		 //McASPTXINT();
		 Audio_Start(); //UARTprintf(" Done for Audio_Start\n ");

//#endif
		// while(1){

		 //I2SDataPutNonBlocking(I2S_BASE, I2S_DATA_LINE_0, tmp[i]);

		 //pTxBuffer->pucWritePtr = tmp[i];
		 //UARTprintf("%x\n\r",pTxBuffer->pucWritePtr);
		 //I2SDataPutNonBlocking(I2S_BASE, I2S_DATA_LINE_0, sin[i%16]);
		// UARTprintf("%x\n\r",sin[i%16]);

	    //I2SDataPut(I2S_BASE, I2S_DATA_LINE_0, ((unsigned long*)buffer)[0]);
	    //I2SDataPutNonBlocking(I2S_BASE, I2S_DATA_LINE_0, ((unsigned long*)buffer)[0]); UARTprintf("Done for I2SDataPutNonBlocking\n");
		 //};
		    // Start Audio Tx/Rx
	    //UARTprintf(" Audio is starting %d\n\r");
	//ControlTaskCreate(); UARTprintf(" Done for ControlTaskCreate\n");

    // Start the Speaker Task
    //
    //osi_TaskCreate( Speaker, (signed char*)"Speaker",OSI_STACK_SIZE, NULL, 1, &g_SpeakerTask );
    //osi_TaskCreate( Speaker, (signed char*)"Speaker",OSI_STACK_SIZE, NULL, 1, &g_pAudioOutControlHdl );
    //UARTprintf(" Done for osi_TaskCreate\n");
	//UARTprintf("%x", pRxBuffer->pucReadPtr);
	//UARTprintf("%x", i);
	//UARTprintf("%x", buffer[i]);
	//UARTprintf("%x", pRxBuffer->pucWritePtr);
	//UARTprintf("%x", pRxBuffer->pucBufferStartPtr);
	//UARTprintf("%x", pRxBuffer->pucBufferEndPtr);
	//UARTprintf("%x", pRxBuffer->)
    //osi_start();
    //UARTprintf(" Done for osi_start\n");
		 vPortFree(audio_buf); //UARTprintf(" audio_buf\n ");
	 return 0;
}

//extern

//unsigned short * record_buf;

int Cmd_record_buff(int argc, char *argv[]) {
#if 0
#define RECORD_SIZE 2
#define minval( a,b ) a < b ? a : b

unsigned long tok;

int err, i ;
long hndl , bytes;
SlFsFileInfo_t info;

unsigned char content[RECORD_SIZE];

			content[0] = 0xAA;
			content[1] = 0x78;

//record_buf = (char*)pvPortMalloc(AUDIO_BUF_SZ);

argv[1] = "TONE";



sl_FsDel(argv[1], 0); UARTprintf("delete such a file\n");

sl_FsGetInfo(argv[1], tok, &info);

//tok = "this is testing this is testing";

if (sl_FsOpen(argv[1], FS_MODE_OPEN_WRITE, &tok, &hndl)) {
UARTprintf("no such a file, trying to create\n");

if (sl_FsOpen(argv[1], FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok, &hndl)) {
UARTprintf("error opening for write\n");

return -1;
														 	 	 	 	 	 	 	 	 	 }
}

get_codec_mic_NAU(); UARTprintf(" Done for get_codec_NAU\n ");

AudioCaptureRendererConfigure(); //UARTprintf(" Done for AudioCaptureRendererConfigure\n ");

//McASPINTRX(); UARTprintf(" Done for McASPINTRX\n ");

AudioCapturerInit_mic(); UARTprintf(" Done for AudioCapturerInit_mic\n ");

#if 0

Audio_Start(); //UARTprintf(" Done for Audio_Start\n ");

#endif

//content = "this is testing this is testing";

//bytes = sl_FsWrite(hndl, info.FileLen, argv[2], strlen(argv[2]));

//UARTprintf("content is shown here: %d  \n", "this is testing this is testing");

//for (i=0; i < sizeof(content); i++){




bytes = sl_FsWrite(hndl, info.FileLen, content, strlen(content));

//}

UARTprintf("wrote to the file %d bytes\n", bytes);

sl_FsClose(hndl, 0, 0, 0);
#endif
#if 0
//////////////////////////////// start with SD card assessment
//#define RECORD_SIZE 4
//unsigned char content[RECORD_SIZE];
unsigned char content[] = {" \r\n"};

	//content[0] = 0xAA;
	content[1] = 0x78;
	content[2] = 0x55;
	content[3] = 0x50;
//	int str_con = strlen(content[1]); UARTprintf("d% string number \n",str_con);
	Cmd_write_record(content);
//UARTprintf("%d arg is ",saver);
		//Cmd_write(2, arg);
	//	UARTprintf("%dth wrote\n",k);
	//	}
UARTprintf(" Done for Cmd_write_record\n ");
//////////////////////////////// edit for SD card assessment
//vPortFree(record_buf); //UARTprintf(" audio_buf\n ");
#endif
// Create RX and TX Buffer
//
pTxBuffer = CreateCircularBuffer(TX_BUFFER_SIZE);
pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);
// Configure Audio Codec
//
ConfigureAudioCodec(CODEC_I2S_WORD_LEN_16);
// Initialize the Audio(I2S) Module
//
AudioCapturerInit();
// Initialize the DMA Module
//
UDMAInit();
UDMAChannelSelect(UDMA_CH4_I2S_RX, NULL);
UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);

//
// Setup the DMA Mode
//
SetupPingPongDMATransferTx();
SetupPingPongDMATransferRx();
// Setup the Audio In/Out
//
AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
AudioCaptureRendererConfigure();

// Start Audio Tx/Rx
//
Audio_Start();

// Start the Control Task
//
ControlTaskCreate();

// Start the Microphone Task
//
osi_TaskCreate( Microphone,(signed char*)"MicroPhone", OSI_STACK_SIZE, NULL, 1, &g_MicTask );
// Start the Speaker Task
//
osi_TaskCreate( Speaker, (signed char*)"Speaker",OSI_STACK_SIZE, NULL, 1, &g_SpeakerTask );
//
// Start the task scheduler
//
osi_start();
// end of DMA
return 0;

}


int Cmd_fs_delete(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	int err;

	if (err = sl_FsDel(argv[1], 0)) {
		UARTprintf("error %d\n", err);
		return -1;
	}

	// Return success.
	return (0);
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
		}

		unix_now = now;
	} else if (last_ntp != 0) {
		ntp = last_ntp + (now - unix_now) / 1000;
		last_ntp = ntp;
		unix_now = now;
	}
	return ntp;
}

int thread_prox(void* unused) {
	while (1) {
		int prox = get_prox();

		UARTprintf("%d\n", prox);

		vTaskDelay( 100 );
	} //try every little bit
}

#define SENSOR_RATE 60

static unsigned int dust_val=0;
static unsigned int dust_cnt=0;
xSemaphoreHandle dust_smphr;

void thread_dust(void* unused) {
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

static xSemaphoreHandle i2c_smphr;

void thread_light(void* unused) {
	while (1) {
		portTickType now = xTaskGetTickCount();
		int light;

		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			light = get_light();
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
		} else {
			vTaskDelay(100);
			continue;
		}

		vTaskDelayUntil(&now, 100 );
	}
}

xQueueHandle data_queue = 0;

void thread_tx(void* unused) {
	data_t data;

	while (1) {
		if( data_queue != 0 && !xQueueReceive( data_queue, &( data ), portMAX_DELAY ) ) {
			vTaskDelay(100);
			continue;
		}

		UARTprintf("sending time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d\n",
				data.time, data.light, data.light_variability, data.light_tonality, data.temp, data.humid,
				data.dust);

		while (!send_data_pb(&data) == 0) {
			do {vTaskDelay(100);} //wait for a connection...
			while( !(sl_status&HAS_IP ) );
		}//try every little bit
	}
}


void thread_sensor_poll(void* unused) {

	//
	// Print some header text.
	//

	data_t data;

	while (1) {
		portTickType now = xTaskGetTickCount();

		data.time = get_time();

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
			data.humid = get_humid();
			data.temp = get_temp();
			xSemaphoreGive(i2c_smphr);
		} else {
			continue;
		}
		UARTprintf("collecting time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d\n",
				data.time, data.light, data.light_variability, data.light_tonality, data.temp, data.humid,
				data.dust);

		    // ...

        xQueueSend( data_queue, ( void * ) &data, 10 );

		UARTprintf("delay...\n");

		vTaskDelayUntil(&now, SENSOR_RATE * configTICK_RATE_HZ);
	}

}

// ==============================================================================
// This function implements the "tasks" command.  It prints a list of the
// current FreeRTOS tasks with information about status, stack usage, etc.
// ==============================================================================
#if ( configUSE_TRACE_FACILITY == 1 )

int Cmd_tasks(int argc, char *argv[]) {
	signed char* pBuffer;

	pBuffer = pvPortMalloc(1024);
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
int Cmd_mel(int argc, char *argv[]) {

    int i;
	short s[1024];

	for (i = 0; i < 1024; ++i) {
		s[i] = fxd_sin(i * 10) / 4 + fxd_sin(i * 20) / 4 + fxd_sin(i * 100) / 4;
	}

//	norm(s, 1024);
	fix_window(s, 1024);
	fftr(s, 10);
	psd(s, 1024);
	mel_freq(s, 1024, 44100 / 512);

	for (i = 0; i < 16; ++i) {
		UARTprintf("%d ", s[i]);
	}
	UARTprintf("\n");
	return (0);
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
		{ "ping", Cmd_ping, "Ping a server" },
		{ "time", Cmd_time, "get ntp time" },
		{ "status", Cmd_status, "status of simple link" },
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
#if ( configUSE_TRACE_FACILITY == 1 )
		{ "tasks", Cmd_tasks, "Report stats of all tasks" },
#endif

		{ "dust", Cmd_dusttest, "dust sensor test" },


		{ "fswr", Cmd_fs_write, "fs write" },
		{ "fsrd", Cmd_fs_read, "fs read" },
		{ "play_ringtone", Cmd_code_playbuff, "play selected ringtone" },
		{ "record_sounds", Cmd_record_buff,"record sounds"},
		{ "fsdl", Cmd_fs_delete, "fs delete" },
		//{ "readout", Cmd_readout_data, "read out sensor data log" },

		{ "sl", Cmd_sl, "start smart config" },
		{ "mode", Cmd_mode, "set the ap/station mode" },
		{ "mel", Cmd_mel, "test the mel calculation" },

		{ "spird", Cmd_spi_read,"spi read" },
		{ "spiwr", Cmd_spi_write, "spi write" },
		{ "spirst", Cmd_spi_reset, "spi reset" },

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
	int nStatus;
	portTickType now;

	//
	// Initialize the UART for console I/O.
	//
	UARTStdioInit(0);

	UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);

	now = xTaskGetTickCount();
	sl_mode = sl_Start(NULL, NULL, NULL);
	unsigned char mac[6];
	unsigned char mac_len;
	sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);

	UARTprintf("\n\nFreeRTOS %s, %d, %s %x%x%x%x%x%x\n",
	tskKERNEL_VERSION_NUMBER, KIT_VER, MORPH_NAME, mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
	UARTprintf("\n? for help\n");
	UARTprintf("> ");

    // SDCARD INITIALIZATION
    // Enable MMCHS, Reset MMCHS, Configure MMCHS, Configure card clock, mount
    MAP_PRCMPeripheralClkEnable(PRCM_SDHOST,PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_SDHOST);
    MAP_SDHostInit(SDHOST_BASE);
    MAP_SDHostSetExpClk(SDHOST_BASE,MAP_PRCMPeripheralClockGet(PRCM_SDHOST),15000000);
    Cmd_mnt(0,0);

    //INIT SPI
    spi_init();

	vTaskDelayUntil(&now, 1000);
	if (sl_mode == ROLE_AP || !sl_status) {
		Cmd_sl(0, 0);
	}

	 data_queue = xQueueCreate( 60, sizeof( data_t ) );
	 vSemaphoreCreateBinary( dust_smphr );
	 vSemaphoreCreateBinary( light_smphr );
	 vSemaphoreCreateBinary( i2c_smphr );

	if (data_queue == 0) {
		UARTprintf("Failed to create the data_queue.\n");
	}


	//xTaskCreate(thread_light, "lightTask", 1 * 1024 / 4, NULL, 3, NULL);
	//xTaskCreate(thread_dust, "dustTask", 256 / 4, NULL, 3, NULL);
	//xTaskCreate(thread_sensor_poll, "pollTask", 1 * 1024 / 4, NULL, 3, NULL);
	//xTaskCreate(thread_tx, "txTask", 2 * 1024 / 4, NULL, 2, NULL);
	//xTaskCreate(thread_ota, "otaTask", 1 * 1024 / 4, NULL, 1, NULL);

	//xTaskCreate(thread_light, "lightTask", 1 * 1024 / 4, NULL, 3, NULL);
	//xTaskCreate(thread_dust, "dustTask", 256 / 4, NULL, 3, NULL);
	//xTaskCreate(thread_sensor_poll, "pollTask", 1 * 1024 / 4, NULL, 3, NULL);
	//xTaskCreate(thread_tx, "txTask", 2 * 1024 / 4, NULL, 2, NULL);
	//xTaskCreate(thread_ota, "otaTask", 1 * 1024 / 4, NULL, 1, NULL);


	//checkFaults();

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
			nStatus = CmdLineProcess(cCmdBuf);

			//
			// Handle the case of bad command.
			//
			if (nStatus == CMDLINE_BAD_CMD) {
				UARTprintf("Bad command!\n");
			}

			//
			// Handle the case of too many arguments.
			//
			else if (nStatus == CMDLINE_TOO_MANY_ARGS) {
				UARTprintf("Too many arguments for command processor!\n");
			}

			//
			// Otherwise the command was executed.  Print the error code if one was
			// returned.
			//
			else if (nStatus != 0) {
				UARTprintf("Command returned error code %d\n", nStatus);
			}

			UARTprintf("> ");
		}
	}
}
