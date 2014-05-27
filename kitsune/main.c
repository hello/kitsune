/******************************************************************************
*
*   Copyright (C) 2013 Texas Instruments Incorporated
*
*   All rights reserved. Property of Texas Instruments Incorporated.
*   Restricted rights to use, duplicate or disclose this code are
*   granted through contract.
*
*   The program may not be used without the written permission of
*   Texas Instruments Incorporated or against the terms and conditions
*   stipulated in the agreement under which this program has been supplied,
*   and under no circumstances can it be used with non-TI connectivity device.
*
******************************************************************************/
//*****************************************************************************
//
//! \addtogroup freertos_demo
//! @{
//
//*****************************************************************************
//*****************************************************************************
//
// main.c - Sample FreeRTOS Application for creating 2 tasks
//
//*****************************************************************************

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
#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <hw_types.h>
#include <hw_ints.h>
#include <interrupt.h>
#include <rom.h>
#include <rom_map.h>
#include <uart.h>
#include <prcm.h>
#include <pin.h>
#include <utils.h>
#include "pinmux.h"
#include "portmacro.h"
#include "uart_logger.h"

/*Simple Link inlcudes */
#include <datatypes.h>
#include <simplelink.h>


//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define UART_PRINT               Report
#define SPAWN_TASK_PRIORITY		 4
//*****************************************************************************
//						GLOBAL VARIABLES
//*****************************************************************************

#if defined(ccs)
	extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
	extern uVectorEntry __vector_table;
#endif

/* The queue used to send strings to the task1. */
xQueueHandle xPrintQueue;

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************
static void vTestTask1( void *pvParameters );
static void vTestTask2( void *pvParameters );

/*
 * Hook functions that can get called by the kernel.
 */
void vApplicationStackOverflowHook( xTaskHandle *pxTask,
                                   signed portCHAR *pcTaskName );
void vApplicationTickHook( void );
void vAssertCalled( const char *pcFile, unsigned long ulLine );
void vApplicationIdleHook( void );
void Display31xxBanner();

//*****************************************************************************
//
//! Display31xxBanner
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
void
Display31xxBanner()
{
    Report("\n\n\n\r");
    Report("\t\tkitsune launch...\n\r");
    Report("\n\n\n\r");

}

//******************************************************************************
//
//! First test task
//!
//! \param pvParameters is the parameter passed to the task while creating it.
//!
//!	This Function
//!		1. Receive message from the Queue and display it on the terminal.
//!
//! \return none
//
//******************************************************************************
void vTestTask1( void *pvParameters )
{
   portCHAR *pcMessage;
    for( ;; )
    {
      /* Wait for a message to arrive. */
      xQueueReceive( xPrintQueue, &pcMessage, portMAX_DELAY );

      UART_PRINT("message = ");
      UART_PRINT(pcMessage);
      UART_PRINT("\n\r");
      UtilsDelay(2000000);
    }
}

//******************************************************************************
//
//! Second test task
//!
//! \param pvParameters is the parameter passed to the task while creating it.
//!
//!	This Function
//!		1. Creates a message and send it to the queue.
//!
//! \return none
//
//******************************************************************************
void vTestTask2( void *pvParameters )
{
   unsigned long ul_2;
   const portCHAR *pcInterruptMessage[4] = {"Welcome","to","CC31xx" 
		   ,"development !\n"};
   ul_2 =0;
      
   for( ;; )
     {
       /* Queue a message for the print task to display on the UART CONSOLE. */
      xQueueSend( xPrintQueue, &pcInterruptMessage[ul_2 % 4], portMAX_DELAY );
      ul_2++;
      UtilsDelay(2000000);
     }
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
  	while(1)
    {

    }
}

//*****************************************************************************
//
//! Application defined idle task hook
//! \param  none
//!
//! \return none
//!
//!
//*****************************************************************************
void
vApplicationIdleHook( void )
{

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

    for( ;; );
}


//*****************************************************************************
//
//! Mandatory MCU Initialization Routine
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
void
MCUInit(void)
{
    unsigned long ulRegVal;

    //
    // DIG DCDC NFET SEL and COT mode disable
    //
    HWREG(0x4402F010) = 0x30031820;
    HWREG(0x4402F00C) = 0x04000000;

    UtilsDelay(32000);

    //
    // ANA DCDC clock config
    //
    HWREG(0x4402F11C) = 0x099;
    HWREG(0x4402F11C) = 0x0AA;
    HWREG(0x4402F11C) = 0x1AA;

    //
    // PA DCDC clock config
    //
    HWREG(0x4402F124) = 0x099;
    HWREG(0x4402F124) = 0x0AA;
    HWREG(0x4402F124) = 0x1AA;

    //
    // Enable RTC
    //
    if(MAP_PRCMSysResetCauseGet()== PRCM_POWER_ON)
    {
        HWREG(0x4402F804) = 0x1;
    }

    //
    // TD Flash timing configurations in case of MCU WDT reset
    //

    if((HWREG(0x4402D00C) & 0xFF) == 0x00000005)
    {
        HWREG(0x400F707C) |= 0x01840082;
        HWREG(0x400F70C4)= 0x1;
        HWREG(0x400F70C4)= 0x0;
    }

    //
    // JTAG override for I2C in SWD mode
    //
    if(((HWREG(0x4402F0C8) & 0xFF) == 0x2))
    {
        MAP_PinModeSet(PIN_19,PIN_MODE_2);
        MAP_PinModeSet(PIN_20,PIN_MODE_2);
        HWREG(0x4402E184) |= 0x1;
    }

    //
    // Take I2C semaphore,##IMPROTANT:REMOVE IN PG1.32 DEVICES##
    //
    ulRegVal = HWREG(COMMON_REG_BASE + COMMON_REG_O_I2C_Properties_Register);
    ulRegVal = (ulRegVal & ~0x3) | 0x1;
    HWREG(COMMON_REG_BASE + COMMON_REG_O_I2C_Properties_Register) = ulRegVal;

    //
    // Take GPIO semaphore##IMPROTANT:REMOVE IN PG1.32 DEVICES##
    //
    ulRegVal = HWREG(COMMON_REG_BASE + COMMON_REG_O_GPIO_properties_register);
    ulRegVal = (ulRegVal & ~0x3FF) | 0x155;
    HWREG(COMMON_REG_BASE + COMMON_REG_O_GPIO_properties_register) = ulRegVal;

    //
    // Change UART pins(55,57) mode to PIN_MODE_0 if they are in PIN_MODE_1(NWP mode)
    //
    if((MAP_PinModeGet(PIN_55)) == PIN_MODE_1)
    {
        MAP_PinModeSet(PIN_55,PIN_MODE_0);
    }
    if((MAP_PinModeGet(PIN_57)) == PIN_MODE_1)
    {
        MAP_PinModeSet(PIN_57,PIN_MODE_0);
    }

}



//*****************************************************************************
//
//!  main function handling the freertos_demo.
//!
//! \param  None
//!
//! \return none
//
//*****************************************************************************
int main( void )
{
    #if defined(ccs)
            IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
    #endif
    #if defined(ewarm)
            IntVTableBaseSet((unsigned long)&__vector_table);
    #endif

    MCUInit();
    //
    // Enables the clock ticking for scheduler to switch between different
    // tasks.
    //
    IntEnable(FAULT_SYSTICK);

    PinMuxConfig();
    //
    // Initializing the terminal
    //
    InitTerm();
    //
    // Clearing the terminal
    //
    ClearTerm();
    //
    // Diasplay Banner
    //
    Display31xxBanner();
    //
    // Creating a queue for 10 elements.
    //
    xPrintQueue =xQueueCreate( 10, sizeof( unsigned portLONG ) );

    if( xPrintQueue == 0 )
    {
      // Queue was not created and must not be used.
      return 0;
    }
    VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
    //
    // Create the Queue Receive task
    //
    osi_TaskCreate( vTestTask1, ( signed portCHAR * ) "TASK1",
                512, NULL, tskIDLE_PRIORITY+1, NULL );
    //
    // Create the Queue Send task
    //
    osi_TaskCreate( vTestTask2, ( signed portCHAR * ) "TASK2",
                512,NULL, tskIDLE_PRIORITY+1, NULL );

    //
    // Start the task scheduler
    //
    osi_start();

    return 0;
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
