/*
 *   Copyright (C) 2015 Texas Instruments Incorporated
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
 */

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"

#include <driverlib/interrupt.h>
#include <driverlib/timer.h>
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>
#include <driverlib/prcm.h>

// TI-RTOS includes
#if defined(USE_TIRTOS) || defined(USE_FREERTOS) || defined(SL_PLATFORM_MULTI_THREADED)
#include <stdlib.h>
#include "osi.h"
#endif

#include "timer_if.h"

static unsigned long g_ulTimerA2Base;


static unsigned char
GetPeripheralIntNum(unsigned long ulBase, unsigned long ulTimer)
{
    if(ulTimer == TIMER_A)
    {
       switch(ulBase)
       {
           case TIMERA0_BASE:
                 return INT_TIMERA0A;
           case TIMERA1_BASE:
                 return INT_TIMERA1A;
           case TIMERA2_BASE:
                 return INT_TIMERA2A;
           case TIMERA3_BASE:
                 return INT_TIMERA3A;
           default:
                 return INT_TIMERA0A;
           }
    }
    else if(ulTimer == TIMER_B)
    {
       switch(ulBase)
       {
           case TIMERA0_BASE:
                 return INT_TIMERA0B;
           case TIMERA1_BASE:
                 return INT_TIMERA1B;
           case TIMERA2_BASE:
                 return INT_TIMERA2B;
           case TIMERA3_BASE:
                 return INT_TIMERA3B;
           default:
                 return INT_TIMERA0B;
           }
    }
    else
    {
        return INT_TIMERA0A;
    }

}

//*****************************************************************************
//
//!    Initializing the Timer
//!
//! \param ePeripheral is the peripheral which need to be initialized.
//! \param ulBase is the base address for the timer.
//! \param ulConfig is the configuration for the timer.
//! \param ulTimer selects amoung the TIMER_A or TIMER_B or TIMER_BOTH.
//! \param ulValue is the timer prescale value which must be between 0 and
//! 255 (inclusive) for 16/32-bit timers and between 0 and 65535 (inclusive)
//! for 32/64-bit timers.
//! This function
//!     1. Enables and reset the peripheral for the timer.
//!     2. Configures and set the prescale value for the timer.
//!
//! \return none
//
//*****************************************************************************
void Timer_IF_Init( unsigned long ePeripheral, unsigned long ulBase, unsigned
               long ulConfig, unsigned long ulTimer, unsigned long ulValue)
{
    //
    // Initialize GPT A0 (in 32 bit mode) as periodic down counter.
    //
    MAP_PRCMPeripheralClkEnable(ePeripheral, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(ePeripheral);
    MAP_TimerConfigure(ulBase,ulConfig);
    MAP_TimerPrescaleSet(ulBase,ulTimer,ulValue);
}

//*****************************************************************************
//
//!    setting up the timer
//!
//! \param ulBase is the base address for the timer.
//! \param ulTimer selects between the TIMER_A or TIMER_B or TIMER_BOTH.
//! \param TimerBaseIntHandler is the pointer to the function that handles the
//!    interrupt for the Timer
//!
//! This function
//!     1. Register the function handler for the timer interrupt.
//!     2. enables the timer interrupt.
//!
//! \return none
//
//*****************************************************************************
void Timer_IF_IntSetup(unsigned long ulBase, unsigned long ulTimer, 
                   void (*TimerBaseIntHandler)(void))
{
  //
  // Setup the interrupts for the timer timeouts.
  //
#if defined(USE_TIRTOS) || defined(USE_FREERTOS) || defined(SL_PLATFORM_MULTI_THREADED) 
    // USE_TIRTOS: if app uses TI-RTOS (either networking/non-networking)
    // USE_FREERTOS: if app uses Free-RTOS (either networking/non-networking)
    // SL_PLATFORM_MULTI_THREADED: if app uses any OS + networking(simplelink)
      if(ulTimer == TIMER_BOTH)
      {
          osi_InterruptRegister(GetPeripheralIntNum(ulBase, TIMER_A),
                                   TimerBaseIntHandler, INT_PRIORITY_LVL_1);
          osi_InterruptRegister(GetPeripheralIntNum(ulBase, TIMER_B),
                                  TimerBaseIntHandler, INT_PRIORITY_LVL_1);
      }
      else
      {
          osi_InterruptRegister(GetPeripheralIntNum(ulBase, ulTimer),
                                   TimerBaseIntHandler, INT_PRIORITY_LVL_1);
      }
        
#else
	  MAP_IntPrioritySet(GetPeripheralIntNum(ulBase, ulTimer), INT_PRIORITY_LVL_1);
      MAP_TimerIntRegister(ulBase, ulTimer, TimerBaseIntHandler);
#endif
 

  if(ulTimer == TIMER_BOTH)
  {
    MAP_TimerIntEnable(ulBase, TIMER_TIMA_TIMEOUT|TIMER_TIMB_TIMEOUT);
  }
  else
  {
    MAP_TimerIntEnable(ulBase, ((ulTimer == TIMER_A) ? TIMER_TIMA_TIMEOUT : 
                                   TIMER_TIMB_TIMEOUT));
  }
}

//*****************************************************************************
//
//!    clears the timer interrupt
//!
//! \param ulBase is the base address for the timer.
//!
//! This function
//!     1. clears the interrupt with given base.
//!
//! \return none
//
//*****************************************************************************
void Timer_IF_InterruptClear(unsigned long ulBase)
{
    unsigned long ulInts;
    ulInts = MAP_TimerIntStatus(ulBase, true);
    //
    // Clear the timer interrupt.
    //
    MAP_TimerIntClear(ulBase, ulInts);
}

//*****************************************************************************
//
//!    starts the timer
//!
//! \param ulBase is the base address for the timer.
//! \param ulTimer selects amoung the TIMER_A or TIMER_B or TIMER_BOTH.
//! \param ulValue is the time delay in mSec after that run out, 
//!                 timer gives an interrupt.
//!
//! This function
//!     1. Load the Timer with the specified value.
//!     2. enables the timer.
//!
//! \return none
//!
//! \Note- HW Timer runs on 80MHz clock 
//
//*****************************************************************************
void Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, 
                unsigned long ulValue)
{
    MAP_TimerLoadSet(ulBase,ulTimer,MILLISECONDS_TO_TICKS(ulValue));
    //
    // Enable the GPT 
    //
    MAP_TimerEnable(ulBase,ulTimer);
}

//*****************************************************************************
//
//!    disable the timer
//!
//! \param ulBase is the base address for the timer.
//! \param ulTimer selects amoung the TIMER_A or TIMER_B or TIMER_BOTH.
//!
//! This function
//!     1. disables the interupt.
//!
//! \return none
//
//*****************************************************************************
void Timer_IF_Stop(unsigned long ulBase, unsigned long ulTimer)
{
    //
    // Disable the GPT 
    //
    MAP_TimerDisable(ulBase,ulTimer);
}

//*****************************************************************************
//
//!    De-Initialize the timer
//!
//! \param uiGPTBaseAddr
//! \param ulTimer
//!
//! This function 
//!        1. disable the timer interrupts
//!        2. unregister the timer interrupt
//!
//!    \return None.
//
//*****************************************************************************
void Timer_IF_DeInit(unsigned long ulBase,unsigned long ulTimer)
{
    //
    // Disable the timer interrupt
    //
    MAP_TimerIntDisable(ulBase,TIMER_TIMA_TIMEOUT|TIMER_TIMB_TIMEOUT);
    //
    // Unregister the timer interrupt
    //
    MAP_TimerIntUnregister(ulBase,ulTimer);
}

//*****************************************************************************
//
//!    starts the timer
//!
//! \param ulBase is the base address for the timer.
//! \param ulTimer selects between the TIMER A and TIMER B.
//! \param ulValue is timer reload value (mSec) after which the timer will run out and gives an interrupt.
//!
//! This function
//!     1. Reload the Timer with the specified value.
//!
//! \return none
//
//*****************************************************************************
void Timer_IF_ReLoad(unsigned long ulBase, unsigned long ulTimer, 
                unsigned long ulValue)
{
    MAP_TimerLoadSet(ulBase,ulTimer,MILLISECONDS_TO_TICKS(ulValue));
}

//*****************************************************************************
//
//!    starts the timer
//!
//! \param ulBase is the base address for the timer.
//! \param ulTimer selects amoung the TIMER_A or TIMER_B or TIMER_BOTH.
//!
//! This function
//!     1. returns the timer value.
//!
//! \return Timer Value.
//
//*****************************************************************************
unsigned int Timer_IF_GetCount(unsigned long ulBase, unsigned long ulTimer)
{
    unsigned long ulCounter;
    ulCounter = MAP_TimerValueGet(ulBase, ulTimer);
    return 0xFFFFFFFF - ulCounter;
}


/* Timer A2 is used to serve the host driver */
void simplelink_timerA2_start()
{
    // Base address for second timer
    //
    g_ulTimerA2Base = TIMERA2_BASE;

    //
    // Configuring the timerA2
    //
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA2,PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_TIMERA2);
    MAP_TimerConfigure(g_ulTimerA2Base,TIMER_CFG_PERIODIC);
    MAP_TimerPrescaleSet(g_ulTimerA2Base,TIMER_A,0);

    /* configure the timer counter load value to max 32-bit value */
    MAP_TimerLoadSet(g_ulTimerA2Base,TIMER_A, 0xFFFFFFFF);
    //
    // Enable the GPT
    //
    MAP_TimerEnable(g_ulTimerA2Base,TIMER_A);


}


/* This function returns the timer counter value in ticks units */
unsigned long TimerGetCurrentTimestamp()
{
        return (0xFFFFFFFF - TimerValueGet(g_ulTimerA2Base,TIMER_A));
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

