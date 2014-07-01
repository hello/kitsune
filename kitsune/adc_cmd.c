
#include <stdio.h>
#include <string.h>

#include <hw_types.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <pin.h>
#include <uart.h>
#include <stdarg.h>
#include <stdlib.h>
#include "rom_map.h"

#include "ustdlib.h"
#include "uartstdio.h"

#include "FreeRTOS.h"
#include "task.h"

#include "adc_cmd.h"
#include "timer.h"
#include "hw_adc.h"
#include "adc.h"

#define UART_PRINT               UARTprintf

#define TIMER_INTERVAL_RELOAD   65535
#define PULSE_WIDTH				2097

#define BUFFER_SZ 4096
unsigned long pulAdcSamples[BUFFER_SZ];

//****************************************************************************
//
//! Setup the timer in PWM mode
//!
//! \param ulBase is the base address of the timer to be configured
//! \param ulTimer is the timer to be setup (TIMER_A or  TIMER_B)
//! \param ulConfig is the timer configuration setting
//! \param ucInvert is to select the inversion of the output
//!
//! This function
//!    1. The specified timer is setup to operate as PWM
//!
//! \return None.
//
//****************************************************************************
void SetupTimerPWMMode(unsigned long ulBase, unsigned long ulTimer,
                       unsigned long ulConfig, unsigned char ucInvert)
{
    //
    // Set GPT - Configured Timer in PWM mode.
    //
    MAP_TimerConfigure(ulBase,ulConfig);
    //
    // Inverting the timer output if required
    //
    MAP_TimerControlLevel(ulBase,ulTimer,ucInvert);
    //
    // Load value set to ~10 ms time period
    //
    MAP_TimerLoadSet(ulBase,ulTimer,TIMER_INTERVAL_RELOAD);
    //
    // Match value set so as to output level 0
    //
    MAP_TimerMatchSet(ulBase,ulTimer,TIMER_INTERVAL_RELOAD);

}
int
Cmd_adctest(int argc, char *argv[])
{
	unsigned long  uiAdcInputPin;
	unsigned int  uiChannel;
    unsigned int  uiIndex=0;
    unsigned long ulSample;
    int i = 0;

	uiAdcInputPin = 0x3b;
	uiChannel = ADC_CH_3;

    //
    // Pinmux for the selected ADC input pin
    //
    PinTypeADC(uiAdcInputPin,0xFF);

    //PWM stuff...
    //
    // Initialization of timers to generate PWM output
    //
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA2, PRCM_RUN_MODE_CLK);
    //
    // TIMERA2 (TIMER B) (red led on launchpad) GPIO 9 --> PWM_5
    //
    SetupTimerPWMMode(TIMERA2_BASE, TIMER_B,
            (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_B_PWM), 1);

    MAP_TimerPrescaleSet(PRCM_TIMERA2, TIMER_B, 82); //prescale to 100hz

    MAP_TimerEnable(TIMERA2_BASE,TIMER_B);

    MAP_TimerMatchSet(TIMERA2_BASE, TIMER_B, PULSE_WIDTH);
    //end pwm

    //
    // Enable ADC channel
    //
    ADCChannelEnable(ADC_BASE, uiChannel);

    //
    // Configure ADC timer which is used to timestamp the ADC data samples
    //
    ADCTimerConfig(ADC_BASE,2^17);

    //
    // Enable ADC timer which is used to timestamp the ADC data samples
    //
    ADCTimerEnable(ADC_BASE);

    //
    // Enable ADC module
    //
    ADCEnable(ADC_BASE);

    vTaskDelay(100);

    while( ++i < 10 ) {
    //
    // Read BUFFER_SZ ADC samples
    //
    while(uiIndex < BUFFER_SZ)
    {
        if(ADCFIFOLvlGet(ADC_BASE, uiChannel))
        {
            ulSample = ADCFIFORead(ADC_BASE, uiChannel);
            pulAdcSamples[uiIndex++] = ulSample;
        }
    }

    uiIndex = 0;

#if 0
    UART_PRINT("\n\rTotal no of 32 bit ADC data printed :BUFFER_SZ \n\r");
    UART_PRINT("\n\rADC data format:\n\r");
    UART_PRINT("\n\rbits[13:2] : ADC sample\n\r");
    UART_PRINT("\n\rbits[31:14]: Time stamp of ADC sample \n\r");
#endif

    //
    // Print out BUFFER_SZ ADC samples
    //
    while(uiIndex < BUFFER_SZ)
    {
        UART_PRINT("0x%x\n\r",pulAdcSamples[uiIndex++]&0x1ffe);
    }
    }
    MAP_TimerDisable(TIMERA2_BASE, TIMER_B);

    return(0);
}/*
 * adc_cmd.c
 *
 *  Created on: Jun 30, 2014
 *      Author: chrisjohnson
 */




