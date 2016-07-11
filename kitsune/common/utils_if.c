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

//*****************************************************************************
//  utils_if.c
//
//  Contains utility routines.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "hw_types.h"
#include "hw_memmap.h"
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>
#include <driverlib/hwspinlock.h>
#include <driverlib/prcm.h>
#include <driverlib/spi.h>

//*****************************************************************************
//
//! \addtogroup utils_if_api
//! @{
//
//*****************************************************************************


//*****************************************************************************
// Defines.
//*****************************************************************************
#define INSTR_READ_STATUS       0x05
#define INSTR_DEEP_POWER_DOWN   0xB9
#define STATUS_BUSY             0x01
#define MAX_SEMAPHORE_TAKE_TRIES (4000000)

//****************************************************************************
//
//! Put SPI flash into Deep Power Down mode
//!
//! Note:SPI flash is a shared resource between MCU and Network processing 
//!      units. This routine should only be exercised after all the network
//!      processing has been stopped. To stop network processing use sl_stop API
//! \param None
//!
//! \return Status, 0:Pass, -1:Fail
//
//****************************************************************************
int Utils_SpiFlashDeepPowerDown(void)
{
    unsigned long ulStatus=0;

    //
    // Acquire SSPI HwSpinlock.
    //
    if (0 != MAP_HwSpinLockTryAcquire(HWSPINLOCK_SSPI, MAX_SEMAPHORE_TAKE_TRIES))
    {
        return -1;
    }

    //
    // Enable clock for SSPI module
    //
    MAP_PRCMPeripheralClkEnable(PRCM_SSPI, PRCM_RUN_MODE_CLK);

    //
    // Reset SSPI at PRCM level and wait for reset to complete
    //
    MAP_PRCMPeripheralReset(PRCM_SSPI);
    while(MAP_PRCMPeripheralStatusGet(PRCM_SSPI)== false)
    {
    }

    //
    // Reset SSPI at module level
    //
    MAP_SPIReset(SSPI_BASE);

    //
    // Configure SSPI module
    //
    MAP_SPIConfigSetExpClk(SSPI_BASE,PRCMPeripheralClockGet(PRCM_SSPI),
                     20000000,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVELOW |
                     SPI_WL_8));

    //
    // Enable SSPI module
    //
    MAP_SPIEnable(SSPI_BASE);

    //
    // Enable chip select for the spi flash.
    //
    MAP_SPICSEnable(SSPI_BASE);

    //
    // Wait for spi flash.
    //
    do
    {
        //
        // Send the status register read instruction and read back a dummy byte.
        //
        MAP_SPIDataPut(SSPI_BASE,INSTR_READ_STATUS);
        MAP_SPIDataGet(SSPI_BASE,&ulStatus);

        //
        // Write a dummy byte then read back the actual status.
        //
        MAP_SPIDataPut(SSPI_BASE,0xFF);
        MAP_SPIDataGet(SSPI_BASE,&ulStatus);
    }
    while((ulStatus & 0xFF )== STATUS_BUSY);

    //
    // Disable chip select for the spi flash.
    //
    MAP_SPICSDisable(SSPI_BASE);

    //
    // Start another CS enable sequence for Power down command.
    //
    MAP_SPICSEnable(SSPI_BASE);

    //
    // Send Deep Power Down command to spi flash
    //
    MAP_SPIDataPut(SSPI_BASE,INSTR_DEEP_POWER_DOWN);

    //
    // Disable chip select for the spi flash.
    //
    MAP_SPICSDisable(SSPI_BASE);

    //
    // Release SSPI HwSpinlock.
    //
    MAP_HwSpinLockRelease(HWSPINLOCK_SSPI);

    //
    // Return as Pass.
    //
    return 0;
}

//****************************************************************************
//
//! Used to trigger a hibernate cycle for the SOC
//!
//! Note:This routine should only be exercised after all the network
//!      processing has been stopped. To stop network processing use sl_stop API
//! \param None
//!
//! \return None
//! Usage : This utility routine can be used to do a clean reboot of device.
//! Note: Utils_TriggerHibCycle() API has removed from SDK, instead use
//!			PRCMTriggerHibernateCycle() API for hibernate.
//
//****************************************************************************


#if 0
void Utils_TriggerHibCycle(void)
{

    //
    // Enable the HIB RTC
    //
    MAP_PRCMHibernateWakeupSourceEnable(PRCM_HIB_SLOW_CLK_CTR);

    //
    // Configure the HIB module RTC wake time
    //
    MAP_PRCMHibernateIntervalSet(100);
    // Note : Any addition of code after this line would need a change in
    // ullTicks parameter passed to MAP_PRCMHibernateIntervalSet

    //
    // Enter HIBernate mode
    //
    MAP_PRCMHibernateEnter();
}
#endif
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
