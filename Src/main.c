//*****************************************************************************
//
// main.c - FreeRTOS porting example on CCS4
//
// Copyright (c) 2006-2010 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
//
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
//
// Modified to work with TI ED-LM4F232 on 5/18/2012 by:
//
//    Ken Pettit
//    Fuel7, Inc.
//
//*****************************************************************************

#include <stdlib.h>
#include <assert.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FatFS for timer */
#include "fatfs/src/diskio.h"

/* Standard Stellaris includes */
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"

/* Other Stellaris include */
#include "grlib/grlib.h"
#include "drivers/cfal96x64x16.h"
#include "utils/cpu_usage.h"

#include "uartstdio.h"

//*****************************************************************************
//
//! <h1>START APPLICATION with FreeRTOS for Texas Instruments/Luminary Micro
//!     EK-LM4F232 evauation-board </h1>
//!
//! - It creates two simple tasks, one writes on the OLED, the other toggles a LED
//! - It uses Stellaris libraries
//
//*****************************************************************************

//*****************************************************************************
// Set mainCREATE_FPU_CONTEXT_SAVE_TEST to 1 to create a test to validate the
// context saving of the FPU registers.
//
// NOTE:  This will cause long context switch times as it performs a nexted
//        interrupt test at each SysTick, saving/restoring FPU registers with
//        each nested ISR.  It should only be enabled for testing purposes. 
//*****************************************************************************
#define mainCREATE_FPU_CONTEXT_SAVE_TEST		0

extern void vRegTestClearFlopRegistersToParameterValue( unsigned long ulValue );
extern unsigned long ulRegTestCheckFlopRegistersContainParameterValue( unsigned long ulValue );

/* The following variables are used to verify that the interrupt nesting depth
is as intended.  ulFPUInterruptNesting is incremented on entry to an interrupt
that uses the FPU, and decremented on exit of the same interrupt.
ulMaxFPUInterruptNesting latches the highest value reached by
ulFPUInterruptNesting.  These variables have no other purpose. */
volatile unsigned long ulFPUInterruptNesting = 0UL, ulMaxFPUInterruptNesting = 0UL;

void TIM0A_IRQHandler( void );
void TIM0B_IRQHandler( void );

//*****************************************************************************
//
// The speed of the processor clock, which is therefore the speed of the clock
// that is fed to the peripherals.
//
//*****************************************************************************
unsigned long g_ulSystemClock;

// ==============================================================================
// The CPU usage in percent, in 16.16 fixed point format.
// ==============================================================================
unsigned long g_ulCPUUsage;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

/*
 * Hook functions that can get called by the kernel.
 */
void vApplicationIdleHook( void );
void vApplicationTickHook( void );
void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed portCHAR *pcTaskName );
void vUARTTask( void *pvParameters );

typedef struct taskParams
{
	xSemaphoreHandle	blinkSema;
} taskParams_t;

taskParams_t* g_pTaskParams = NULL;

/*-----------------------------------------------------------*/
void vBlinkTask( void *pvParameters )
{
    volatile unsigned long ul;
    static long toggle = 0x00;
	taskParams_t*	pTaskParams = (taskParams_t *) pvParameters;    

    /* As per most tasks, this task is implemented in an infinite loop. */
    for( ;; )
    {
        toggle ^= 0x01;
        if (toggle)
        {
            GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, GPIO_PIN_2);
        }
        else
        {
            GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);
        }

		/* Wait for a signal from the timing task */
 		xSemaphoreTake(pTaskParams->blinkSema, portMAX_DELAY);
    }
}

/*-----------------------------------------------------------*/
void vTimingTask( void *pvParameters )
{
	taskParams_t*	pTaskParams = (taskParams_t *) pvParameters;
	unsigned short	usCount = 0;

	while (1)
	{
		// Sleep for 10ms
		vTaskDelay(10 / portTICK_RATE_MS);
		
		// Every 500ms, signal the FlashTask
		if (++usCount >= 50)
		{
			usCount = 0;
			xSemaphoreGive(pTaskParams->blinkSema);
		}
		//
		// Call the FatFs tick timer every 10ms.
		//
		disk_timerproc();
	}
}


/*-----------------------------------------------------------*/
int main(void)
{
    //
    // Set the clocking to run at 66.66MHz from the PLL.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_3 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

    // Get the system clock speed.
    g_ulSystemClock = SysCtlClockGet();

    // Initialize the CPU usage measurement routine.
    CPUUsageInit(g_ulSystemClock, configTICK_RATE_HZ/10, 1);

    // Enable SSI0 for sdcard
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    
    //
    // Configure GPIO Pin used for the LED.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2);

    // Turn off the LED.
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	#if ( mainCREATE_FPU_CONTEXT_SAVE_TEST == 1 )
	{
		SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
		TimerIntRegister(TIMER0_BASE, TIMER_A, TIM0A_IRQHandler);
		TimerIntRegister(TIMER0_BASE, TIMER_B, TIM0B_IRQHandler);
		TimerIntEnable(TIMER0_BASE, 0);
	}
	#endif
	
	/*-------------------------------------------
	     Create semaphores for inter-task
	     signaling
	-------------------------------------------*/
	g_pTaskParams = pvPortMalloc(sizeof(taskParams_t));
	vSemaphoreCreateBinary(g_pTaskParams->blinkSema);

    /*-------------------------------------------
         Create task and start scheduler
    -------------------------------------------*/

    /* Create the LED blink task. */
    xTaskCreate(    vBlinkTask, /* Pointer to the function that implements the task. */
                    "vBlinkTask",/* Text name for the task.  This is to facilitate debugging only. */
                    configMINIMAL_STACK_SIZE,          /* Stack depth in words. */
                    g_pTaskParams,/* Pass in our parameter pointer */
                    3,            /* This task will run at priority 1. */
                    NULL );       /* We are not using the task handle. */
   
    /* Create the timing conrol task. */
    xTaskCreate( vTimingTask, "TimingTask", configMINIMAL_STACK_SIZE, g_pTaskParams, 2, NULL );

    /* Create the UART processing task. */
    xTaskCreate( vUARTTask, "UARTTask", 200, g_pTaskParams, 2, NULL );

    /* Start the scheduler so our tasks start executing. */
    vTaskStartScheduler();

    /* If all is well we will never reach here as the scheduler will now be
    running.  If we do reach here then it is likely that there was insufficient
    heap available for the idle task to be created. */
    while (1)
    {
    }
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	SysCtlSleep();
}

/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	static unsigned char count = 0;
	
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
    
	if (++count == 10)
	{
    	g_ulCPUUsage = CPUUsageTick();
    	count = 0;
	}

	#if ( mainCREATE_FPU_CONTEXT_SAVE_TEST == 1 )
	{
		/* Just to verify that the interrupt nesting behaves as expected,
		increment ulFPUInterruptNesting on entry, and decrement it on exit. */
		ulFPUInterruptNesting++;

		/* Fill the FPU registers with 0. */
		vRegTestClearFlopRegistersToParameterValue( 0UL );
		
		/* Trigger a timer 2 interrupt, which will fill the registers with a
		different value and itself trigger a timer 3 interrupt.  Note that the
		timers are not actually used.  The timer 2 and 3 interrupt vectors are
		just used for convenience. */
		IntPendSet( INT_TIMER0A );
	
		/* Ensure that, after returning from the nested interrupts, all the FPU
		registers contain the value to which they were set by the tick hook
		function. */
		assert( ulRegTestCheckFlopRegistersContainParameterValue( 0UL ) );
		
		ulFPUInterruptNesting--;
	}
	#endif
	
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1 | GPIO_PIN_0, 0);
}

/*-----------------------------------------------------------*/

void TIM0B_IRQHandler( void )
{
	/* Just to verify that the interrupt nesting behaves as expected, increment
	ulFPUInterruptNesting on entry, and decrement it on exit. */
	ulFPUInterruptNesting++;
	
	/* This is the highest priority interrupt in the chain of forced nesting
	interrupts, so latch the maximum value reached by ulFPUInterruptNesting.
	This is done purely to allow verification that the nesting depth reaches
	that intended. */
	if( ulFPUInterruptNesting > ulMaxFPUInterruptNesting )
	{
		ulMaxFPUInterruptNesting = ulFPUInterruptNesting;
	}

	/* Fill the FPU registers with 99 to overwrite the values written by
	TIM0A_IRQHandler(). */
	vRegTestClearFlopRegistersToParameterValue( 99UL );
	
	ulFPUInterruptNesting--;
}
/*-----------------------------------------------------------*/

void TIM0A_IRQHandler( void )
{
	/* Just to verify that the interrupt nesting behaves as expected, increment
	ulFPUInterruptNesting on entry, and decrement it on exit. */
	ulFPUInterruptNesting++;
	
	/* Fill the FPU registers with 1. */
	vRegTestClearFlopRegistersToParameterValue( 1UL );
	
	/* Trigger a timer 3 interrupt, which will fill the registers with a
	different value. */
	IntPendSet( INT_TIMER0B );

	/* Ensure that, after returning from the nesting interrupt, all the FPU
	registers contain the value to which they were set by this interrupt
	function. */
	assert( ulRegTestCheckFlopRegistersContainParameterValue( 1UL ) );
	
	ulFPUInterruptNesting--;
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* This function will only be called if an API call to create a task, queue
    or semaphore fails because there is too little heap RAM remaining. */
    for( ;; );
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed portCHAR *pcTaskName )
{
    /* This function will only be called if a task overflows its stack.  Note
    that stack overflow checking does slow down the context switch
    implementation. */
    UARTprintf("FATAL!  Stack overflow in task %s\n", pcTaskName);
    
    for( ;; );
}



