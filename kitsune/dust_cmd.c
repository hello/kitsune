/*
 * dust_cmd.c
 *
 *  Created on: Jun 30, 2014
 *      Author: chrisjohnson
 */

#include <stdio.h>
#include <string.h>

#include <hw_types.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <pin.h>
#include <uart.h>
#include <stdarg.h>
#include <stdlib.h>

#include "hw_types.h"
#include "hw_ints.h"
#include "rom.h"
#include "gpio.h"
#include "interrupt.h"
#include "utils.h"
#include "rom_map.h"

#include "ustdlib.h"
#include "uartstdio.h"

#include "FreeRTOS.h"
#include "task.h"

#include "dust_cmd.h"
#include "timer.h"
#include "hw_adc.h"
#include "adc.h"

#include "led_cmd.h"

#define UART_PRINT               LOGI

#define TIMER_INTERVAL_RELOAD   65535
//#define PULSE_WIDTH             20000//8192//2097
//Pulse is supposed to be 320us
#define PULSE_WIDTH             25600//8192//2097

//#define SAMPLES 4096u
#define SAMPLES 1024u	//grab fewer samples.  This will still encompass
						//at least one dust sensor window

void init_dust() {
	unsigned long uiAdcInputPin;
	unsigned int uiChannel;

	uiAdcInputPin = 0x3b;
	uiChannel = ADC_CH_3;
//
// Pinmux for the selected ADC input pin
//
	PinTypeADC(uiAdcInputPin, 0xFF);

//
// Enable ADC channel
//
	ADCChannelEnable(ADC_BASE, uiChannel);

//
// Configure ADC timer which is used to timestamp the ADC data samples
//
	ADCTimerConfig(ADC_BASE, 2 ^ 17);

}
unsigned int get_dust() {
	unsigned long ulSample;

	unsigned int uiChannel = ADC_CH_3;
//
	unsigned long max, min;
	max = 0;
	min = 99999;
//
//// Read BUFFER_SZ ADC samples
////
//#define DEBUG_DUST 0
//
//#if DEBUG_DUST
//	unsigned short * smplbuf = (unsigned short*)pvPortMalloc(samples*sizeof(short));
//#endif
//	vTaskDelay(1);//yield before we disable the interrupts in case a higher priority task needs to run...
//	unsigned int flags = MAP_IntMasterDisable();
//
//	if (!led_is_idle(0)) {
//		if (!flags) {
//			MAP_IntMasterEnable();
//		}
//		return DUST_SENSOR_NOT_READY;
//	}
//
//	init_dust();
//
//	MAP_GPIOPinWrite(GPIOA1_BASE, 0x2, 0);
//	ADCEnable(ADC_BASE);
//
//	#define ITER 250
//	int i;
//	for(i=1;i!=ITER;++i) { //.32ms of increasing pulse density...
//		HWREG(GPIOA1_BASE + (0x2 << 2)) = 0x2; //avoid the overhead of a function...
//
//		if (i > (ITER/4)) {
//			if (ADCFIFOLvlGet(ADC_BASE, uiChannel)) {
//				ulSample = (ADCFIFORead(ADC_BASE, uiChannel) & 0x3FFC) >> 2;
//				if (ulSample > max) {
//					max = ulSample;
//				}
//				if (ulSample < min) {
//					min = ulSample;
//				}
//				//LOGI("%d\n", ulSample);
//			}
//			UtilsDelay(12);
//		} else {
//			volatile int dly = i>>4;
//			while( dly != 0 ) {--dly;}
//			HWREG(GPIOA1_BASE + (0x2 << 2)) = 0;
//		}
//	}
//
//	MAP_GPIOPinWrite(GPIOA1_BASE, 0x2, 0);
//	ADCDisable(ADC_BASE);
//	if (!flags) {
//		MAP_IntMasterEnable();
//	}
//
//#if DEBUG_DUST
//	if( smplbuf ){
//		int i;
//		LOGF("0,%u\n", xTaskGetTickCount() );
//		for(i=0;i<samples;++i) {
//			LOGF("%u,\n", smplbuf[i]);
//			if(i%100) {
//				vTaskDelay(1);
//			}
//		}
//		vPortFree(smplbuf);
//	}
//#endif

	return max;
}
unsigned int median_filter(unsigned int x, unsigned int * buf,unsigned int * p);
int Cmd_dusttest(int argc, char *argv[]) {
	unsigned int cnt = atoi(argv[1]);
//	if( argc == 1 ) {cnt=100;}
//	unsigned int total = cnt;
//	unsigned int sum = 0;
//	unsigned int filter_buf[3];
//	unsigned int filter_idx=0;
//
//	while( --cnt ) {
//		sum += median_filter(get_dust(), filter_buf, &filter_idx);
//		vTaskDelay(10);
//	}
//	LOGF("%d\n", sum / total );
	return (0);
}

