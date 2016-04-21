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
#include "hellofilesystem.h"
//#include "sl_sync_include_after_simplelink_header.h" not here, this one is operating before the scheduler starts...


#ifdef _ENABLE_SYSVIEW
void SEGGER_SYSVIEW_Conf(void);
void SEGGER_RTT_Init(void);
void SEGGER_SYSVIEW_Start(void);
#endif

void mcu_reset();

extern void vUARTTask(void *pvParameters);

//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define UART_PRINT               Report
#define SPAWN_TASK_PRIORITY		 3

//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
void vAssertCalled( const char *s );
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

int send_top(char * s, int n);
extern tskTCB * volatile pxCurrentTCB;
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
vAssertCalled( const char * s )
{
  LOGE( "%s ASSERT\n", s );
  LOGE( "%s\n", pxCurrentTCB->pcTaskName );
  void* p;
  LOGE("stack ptr %x\n", (int*)&p);
  LOGE( "%x %x\n", pxCurrentTCB->pxStack, pxCurrentTCB->pxTopOfStack );
#if 0
  volatile StackType_t * top = pxCurrentTCB->pxTopOfStack;
  StackType_t * bottom =  pxCurrentTCB->pxStack;

  while( top != bottom ) {
	  LOGE( "%08X\n", *top-- );
	    UtilsDelay(10000);
  }
#endif
  /*
   * yes, this looks like a race, and it is. In the event something goes wrong in the
   * flush, which is likely given it accesses shared data and the filesystem
   * and the system is now in some unkown bad state, this will come in and reset
   * after a half second....
   */
  mcu_reset();
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
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;

    LOGE( "%s STACK OVERFLOW", pcTaskName );
    mcu_reset();
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
	vAssertCalled("WATCHDOG");
}


void start_wdt() {
    //
    // Enable the peripherals used by this example.
    //
    MAP_PRCMPeripheralClkEnable(PRCM_WDT, PRCM_RUN_MODE_CLK);

    //
    // Set up the watchdog interrupt handler.
    //
    WDT_IF_Init(WatchdogIntHandler, 0xFFFFFFFF);

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
#include "kit_assert.h"
volatile portTickType last_upload_time = 0;
#define NWP_WATCHDOG_TIMEOUT
#define ONE_HOUR (1000*60*60)
#define FIFTEEN_MINUTES (1000*60*15)
#define TWENTY_FIVE_HOURS (ONE_HOUR*25)
#ifdef NWP_WATCHDOG_TIMEOUT
void nwp_reset_thread(void* unused) {
	nwp_reset();
	vTaskDelete(NULL);
}
#endif

void watchdog_thread(void* unused) {
#ifdef NWP_WATCHDOG_TIMEOUT
	int last_nwp_reset_time = 0;
#endif
	while (1) {
		if (xTaskGetTickCount() - last_upload_time > 3*ONE_HOUR) {
			LOGE("NET TIMEOUT\n");
			mcu_reset();
		}
#ifdef NWP_WATCHDOG_TIMEOUT
		if (xTaskGetTickCount() - last_upload_time > FIFTEEN_MINUTES
				&& xTaskGetTickCount() - last_nwp_reset_time > FIFTEEN_MINUTES) {
			LOGE("NWP TIMEOUT\n");
			xTaskCreate(nwp_reset_thread, "nwp_reset_thread",
					1280/(sizeof(portSTACK_TYPE)), NULL, 1, NULL);
			last_nwp_reset_time = xTaskGetTickCount();
		}
#endif
		MAP_WatchdogIntClear(WDT_BASE); //clear wdt
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
  // Set the SD card clock as output pin
  //
  MAP_PinDirModeSet(PIN_07,PIN_DIR_MODE_OUT);

#ifdef _ENABLE_SYSVIEW
  SEGGER_RTT_Init();
  SEGGER_SYSVIEW_Conf();
#endif
  //
  // Start the SimpleLink Host
  //

  VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

  wifi_status_init();

  /* Create the UART processing task. */
  xTaskCreate( vUARTTask, "UARTTask", 1024/(sizeof(portSTACK_TYPE)), NULL, 3, NULL );
  xTaskCreate( watchdog_thread, "wdtTask", 512/(sizeof(portSTACK_TYPE)), NULL, 1, NULL );

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
