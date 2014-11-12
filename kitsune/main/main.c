//*****************************************************************************
// main.h
//
// demonstrates STATION mode on CC3200 device
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//****************************************************************************
//
//! \addtogroup getting_started_sta
//! @{
//
//****************************************************************************


/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "osi.h"

/* Hardware library includes. */
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_wdt.h"
#include "wdt.h"
#include "wdt_if.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "uart.h"
#include "prcm.h"
#include "pin.h"
#include "utils.h"
#include "pinmux.h"
#include "portmacro.h"
#include "uart_if.h"
#include "systick.h"

/*Simple Link inlcudes */
#include "simplelink.h"
#include "protocol.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* HW interfaces */
#include "uartstdio.h"
#include "i2c_if.h"

#include "wifi_cmd.h"
#include "uart_logger.h"

extern void vUARTTask( void *pvParameters );

	
//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define UART_PRINT               Report
#define SPAWN_TASK_PRIORITY		 4

//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
void vAssertCalled( const char *pcFile, unsigned long ulLine );
void vApplicationIdleHook();


#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif



//*****************************************************************************
//
//! Application defined idle task hook
//! 
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void
vApplicationIdleHook( void)
{

}
//*****************************************************************************
//
//! Application defined hook (or callback) function - the tick hook.
//! The tick interrupt can optionally call this
//! \param  none
//!
//! \return none
//!
//!
//*****************************************************************************
void
vApplicationTickHook( void )
{
}

//*****************************************************************************
//
//! Application for handling the assertion.
//! \param  none
//!
//! \return none
//!
//!
//*****************************************************************************
void
vAssertCalled( const char *pcFile, unsigned long ulLine )
{

    UARTprintf( "%s %u ASSERT", pcFile, ulLine );

  	while(1)
    {

    }
}

//*****************************************************************************
//
//! Application provided stack overflow hook function.
//!
//! \param  handle and name of the offending task
//!
//! \return none
//!
//*****************************************************************************
void
vApplicationStackOverflowHook( xTaskHandle *pxTask, signed portCHAR *pcTaskName )
{
    ( void ) pxTask;
    ( void ) pcTaskName;

    UARTprintf( "%s STACK OVERFLOW", pcTaskName );

    for( ;; );
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs) || defined(gcc)
    IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enables the clock ticking for scheduler to switch between different
    // tasks.
    //
    // todo figure out why this breaks under sdk 0p5
    //SysTickPeriodSet(configCPU_CLOCK_HZ/configSYSTICK_CLOCK_HZ);
    //SysTickEnable();

    // I2C Init
    //
    I2C_IF_Open(I2C_MASTER_MODE_STD);
	
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

void WatchdogIntHandler(void)
{
	//
	// watchdog interrupt - if it fires when the interrupt has not been cleared then the device will reset...
	//
		UARTprintf( "oh no WDT: %u, %u\r\n", xTaskGetTickCount() );
}


void start_wdt() {
#define WD_PERIOD_MS 				20000
#define MAP_SysCtlClockGet 			80000000
#define LED_GPIO             		MCU_RED_LED_GPIO	/* RED LED */
#define MILLISECONDS_TO_TICKS(ms) 	((MAP_SysCtlClockGet / 1000) * (ms))
    //
    // Enable the peripherals used by this example.
    //
    MAP_PRCMPeripheralClkEnable(PRCM_WDT, PRCM_RUN_MODE_CLK);

    //
    // Set up the watchdog interrupt handler.
    //
    WDT_IF_Init(WatchdogIntHandler, MILLISECONDS_TO_TICKS(WD_PERIOD_MS));

    //
    // Start the timer. Once the timer is started, it cannot be disable.
    //
    MAP_WatchdogEnable(WDT_BASE);
    if(!MAP_WatchdogRunning(WDT_BASE))
    {
       WDT_IF_DeInit();
    }
}
void mcu_reset();
void watchdog_thread(void* unused) {
	int upload_fail_cnt;
	while (1) {
		MAP_WatchdogIntClear(WDT_BASE); //clear wdt
		if (!(sl_status & UPLOADING)) {
			if(++upload_fail_cnt > 60 * 60 ) {
				mcu_reset();
			}
		} else {
			upload_fail_cnt = 0;
		}
		vTaskDelay(1000);
	}
}
//*****************************************************************************
//							MAIN FUNCTION
//*****************************************************************************
void main()
{
  //
  // Board Initialization

  //
  BoardInit();

  start_wdt();
  //
  // configure the GPIO pins for LEDvs
  //
  PinMuxConfig();

  //
  // Initialize the UART for console I/O.
  //
  uart_logger_init();
  UARTStdioInit(0);
  //
  // Set the SD card clock as output pin
  //
  MAP_PinDirModeSet(PIN_07,PIN_DIR_MODE_OUT);

  //
  // Start the SimpleLink Host
  //

  VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

  /* Create the UART processing task. */
  xTaskCreate( vUARTTask, "UARTTask", 1024/(sizeof(portSTACK_TYPE)), NULL, 10, NULL );
  xTaskCreate( watchdog_thread, "wdtTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL );

  //
  // Start the task scheduler
  //
  vTaskStartScheduler();

}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
