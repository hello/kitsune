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

#include <stdio.h>
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"

#include "prcm.h"
#include "wdt.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"

#include "wdt_if.h"
#if defined(USE_TIRTOS) || defined(USE_FREERTOS) || defined(SL_PLATFORM_MULTI_THREADED)
#include "osi.h"
#endif
/****************************************************************************/
/*                       FUNCTION DEFINITIONS                               */
/****************************************************************************/

//****************************************************************************
//
//! Initialize the watchdog timer
//!
//! \param fpAppWDTCB is the WDT interrupt handler to be registered
//! \param uiReloadVal is the reload value to be set to the WDT
//! 
//! This function  
//!        1. Initializes the WDT
//!
//! \return None.
//
//****************************************************************************
void WDT_IF_Init(fAPPWDTDevCallbk fpAppWDTCB,
                   unsigned int uiReloadVal)
{
    //
    // Enable the peripherals used by this example.
    //
    MAP_PRCMPeripheralClkEnable(PRCM_WDT, PRCM_RUN_MODE_CLK);

    //
    // Unlock to be able to configure the registers
    //
    MAP_WatchdogUnlock(WDT_BASE);

    if(fpAppWDTCB != NULL)
    {
#if defined(USE_TIRTOS) || defined(USE_FREERTOS) || defined(SL_PLATFORM_MULTI_THREADED) 
        // USE_TIRTOS: if app uses TI-RTOS (either networking/non-networking)
        // USE_FREERTOS: if app uses Free-RTOS (either networking/non-networking)
        // SL_PLATFORM_MULTI_THREADED: if app uses any OS + networking(simplelink)
        osi_InterruptRegister(INT_WDT, fpAppWDTCB, INT_PRIORITY_LVL_1);
#else
		MAP_IntPrioritySet(INT_WDT, INT_PRIORITY_LVL_1);
        MAP_WatchdogIntRegister(WDT_BASE,fpAppWDTCB);
#endif
    }

    //
    // Set the watchdog timer reload value
    //
    MAP_WatchdogReloadSet(WDT_BASE,uiReloadVal);

    //
    // Start the timer. Once the timer is started, it cannot be disable.
    //
    MAP_WatchdogEnable(WDT_BASE);

}
//****************************************************************************
//
//! DeInitialize the watchdog timer
//!
//! \param None
//! 
//! This function  
//!        1. DeInitializes the WDT
//!
//! \return None.
//
//****************************************************************************
void WDT_IF_DeInit()
{
    //
    // Unlock to be able to configure the registers
    //
    MAP_WatchdogUnlock(WDT_BASE);

    //
    // Disable stalling of the watchdog timer during debug events
    //
    MAP_WatchdogStallDisable(WDT_BASE);

    //
    // Clear the interrupt
    //
    MAP_WatchdogIntClear(WDT_BASE);

    //
    // Unregister the interrupt
    //
    MAP_WatchdogIntUnregister(WDT_BASE);
}
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
