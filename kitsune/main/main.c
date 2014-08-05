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
#include "datatypes.h"
#include "simplelink.h"
#include "protocol.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* HW interfaces */
#include "gpio_if.h"
#include "uartstdio.h"
#include "i2c_if.h"

#include "wifi_cmd.h"

extern void vUARTTask( void *pvParameters );
extern void thead_sensor_poll( void * Data);
	
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

    //setup i2c clock
    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);

    //setup i2c clock
    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);

    //
    // Configure PIN_01 for I2C0 I2C_SCL
    //
    MAP_PinTypeI2C(PIN_01, PIN_MODE_1);

    //
    // Configure PIN_02 for I2C0 I2C_SDA
    //
    MAP_PinTypeI2C(PIN_02, PIN_MODE_1);

    // I2C Init
    //
    I2C_IF_Open(I2C_MASTER_MODE_FST);
	
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}


//*****************************************************************************
//							MAIN FUNCTION
//*****************************************************************************
void main()
{

    unsigned char policyVal;

  //
  // Board Initialization
  //
  BoardInit();
  //
  // configure the GPIO pins for LEDs
  //
  PinMuxConfig();

  //
  // Set the SD card clock as output pin
  //
  MAP_PinDirModeSet(PIN_07,PIN_DIR_MODE_OUT);

  //
  // Start the SimpleLink Host
  //
  VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

  /* Create the UART processing task. */
  xTaskCreate( vUARTTask, "UARTTask", 10*1024/(sizeof(portSTACK_TYPE)), NULL, 2, NULL );
//  xTaskCreate( vWifiTask, "wifi", 2*1024/(sizeof(portSTACK_TYPE)), NULL, 2, NULL );

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
