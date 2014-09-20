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

#define NUM_LOGS 72

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

int thread_prox(void* unused) {
	while (1) {
		int prox = get_prox();

		UARTprintf("%d\n", prox);

		vTaskDelay( 100 );
	} //try every little bit
	return 0;
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

 xSemaphoreHandle i2c_smphr;

void thread_fast_i2c_poll(void* unused) {
	int last_prox =0;
	while (1) {
		portTickType now = xTaskGetTickCount();
		int light;
		int prox,hpf_prox;

		if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
			vTaskDelay(2);
			light = get_light();
			vTaskDelay(2); //this is important! If we don't do it, then the prox will stretch the clock!
			prox = get_prox();
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
		if( data_queue != 0 && !xQueueReceive( data_queue, &( data ), portMAX_DELAY ) ) {
			vTaskDelay(100);
			continue;
		}

		UARTprintf("sending time %d\tlight %d, %d, %d\ttemp %d\thumid %d\tdust %d\n",
				data.time, data.light, data.light_variability, data.light_tonality, data.temp, data.humid,
				data.dust);

		while (!send_periodic_data(&data) == 0) {
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

static void AudioFeatCallback(const int32_t * mfccfeats, const Segment_t * pSegment) {
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

#define NUM_LED 12
#define LED_GPIO_BIT 0x1
#define LED_GPIO_BASE GPIOA3_BASE
int led( unsigned int* color ) {
	int i;
	unsigned long ulInt;
	unsigned int * end = color + NUM_LED;
	//
	// Temporarily turn off interrupts.
	//
	ulInt = MAP_IntMasterDisable();
	for( ;; ) {
		for (i = 0; i < 24; ++i) {
			if ((*color << i) & 0x800000) {
				//1
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_GPIO_BIT);
				UtilsDelay(7);
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, 0x0);
				//UtilsDelay(1);
			} else {
				//0
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_GPIO_BIT);
				UtilsDelay(2);
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, 0x0);
				UtilsDelay(4);
			}
		}
		if( ++color == end ) {
			break;
		}
	}
	if (!ulInt) {
		MAP_IntMasterEnable();
	}
	return 0;
}

int Cmd_led(int argc, char *argv[]) {
	int i;
	unsigned int color[NUM_LED];
	for (i = 0; i < NUM_LED; ++i) {
		color[i] = 0x000fff << i;
	}
	led(color);
	for (i = 0; i < NUM_LED; ++i) {
		vTaskDelay(1000);
		for (i = 0; i < NUM_LED; ++i) {
			color[i] = color[(i + 1) % NUM_LED];
		}
		led(color);
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
#if ( configUSE_TRACE_FACILITY == 1 )
		{ "tasks", Cmd_tasks, "Report stats of all tasks" },
#endif

		{ "dust", Cmd_dusttest, "dust sensor test" },

		{ "sl", Cmd_sl, "start smart config" },
		{ "mode", Cmd_mode, "set the ap/station mode" },
		{ "mel", Cmd_mel, "test the mel calculation" },

		{ "spird", Cmd_spi_read,"spi read" },
		{ "spiwr", Cmd_spi_write, "spi write" },
		{ "spirst", Cmd_spi_reset, "spi reset" },

		{ "antsel", Cmd_antsel, "select antenna" },
		{ "led", Cmd_led, "led test pattern" },

		{ "rssi", Cmd_rssi, "scan rssi" },


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

	if (data_queue == 0) {
		UARTprintf("Failed to create the data_queue.\n");
	}

#if 0
	xTaskCreate(thread_fast_i2c_poll, "fastI2CPollTask", 2 * 1024 / 4, NULL, 3, NULL);
	UARTprintf("*");
	xTaskCreate(thread_dust, "dustTask", 256 / 4, NULL, 3, NULL);
	UARTprintf("*");
	xTaskCreate(thread_sensor_poll, "pollTask", 2 * 1024 / 4, NULL, 4, NULL);
	UARTprintf("*");
	xTaskCreate(thread_tx, "txTask", 4 * 1024 / 4, NULL, 2, NULL);
	UARTprintf("*");
	xTaskCreate(thread_ota, "otaTask", 2 * 1024 / 4, NULL, 1, NULL);
	UARTprintf("*");
#endif
	//checkFaults();

	UARTprintf("\n\nFreeRTOS %s, %d, %s %x%x%x%x%x%x\n",
	tskKERNEL_VERSION_NUMBER, KIT_VER, MORPH_NAME, mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
	UARTprintf("\n? for help\n");
	UARTprintf("> ");

	/* Loop forever */
	while (1) {
		/* remove anything we recieved before we were ready */
		UARTFlushRx();

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
