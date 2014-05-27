
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
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_uart.h"
#include "hw_types.h"
#include "hw_udma.h"

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Hardware library includes. */
#include "rom.h"
#include "rom_map.h"
#include <gpio.h>
#include <udma.h>
#include "interrupt.h"
#include <prcm.h>
#include "uart.h"

/* Peripheral interface includes. */
#include <udma_if.h>

//*****************************************************************************
//
//! \addtogroup audioplayer_app
//! @{
//
//*****************************************************************************
tDMAControlTable gpCtlTbl[CTL_TBL_SIZE];
tAppCallbackHndl gfpAppCallbackHndl[MAX_NUM_CH];




//*****************************************************************************
//
//! Initialize the DMA controller
//!
//! \param None
//! 
//! This function initializes 
//!        1. Initializes the McASP module
//!
//! \return None.
//
//*****************************************************************************
void UDMAInit()
{
    unsigned int uiLoopCnt;
    //
    // Enable McASP at the PRCM module
    //
    PRCMPeripheralClkEnable(PRCM_UDMA,PRCM_RUN_MODE_CLK);
    PRCMPeripheralReset(PRCM_UDMA);

    //
    // Enable uDMA using master enable
    //
    MAP_uDMAEnable();

    //
    // Set Control Table
    //
    memset(gpCtlTbl,0,sizeof(tDMAControlTable)*CTL_TBL_SIZE);
    MAP_uDMAControlBaseSet(gpCtlTbl);
    //
    // Reset App Callbacks
    //
    for(uiLoopCnt = 0; uiLoopCnt < MAX_NUM_CH; uiLoopCnt++)
    {
        gfpAppCallbackHndl[uiLoopCnt] = NULL;
    }
}

//*****************************************************************************
//
//! Configures the uDMA channel
//!
//! \param uiChannel is the DMA channel to be selected
//! \param pfpAppCb is the application callback to be invoked on transfer
//! 
//! This function  
//!        1. Configures the uDMA channel
//!
//! \return None.
//
//*****************************************************************************
void UDMAChannelSelect(unsigned int uiChannel, tAppCallbackHndl pfpAppCb)
{
    if((uiChannel & 0xFF) > MAX_NUM_CH)
    {
        return;
    }
    MAP_uDMAChannelAssign(uiChannel);
    MAP_uDMAChannelAttributeDisable(uiChannel,UDMA_ATTR_ALTSELECT);

    gfpAppCallbackHndl[(uiChannel & 0xFF)] = pfpAppCb;
}

//*****************************************************************************
//
//! Does the actual Memory transfer
//!
//! \param TBD
//! 
//! This function  
//!        1. Sets up the uDMA registers to perform the actual transfer
//!
//! \return None.
//
//*****************************************************************************
void SetupTransfer(
                  unsigned long ulChannel, 
                  unsigned long ulMode,
                  unsigned long ulItemCount,
                  unsigned long ulItemSize, 
                  unsigned long ulArbSize,
                  void *pvSrcBuf, 
                  unsigned long ulSrcInc,
                  void *pvDstBuf, 
                  unsigned long ulDstInc)
{

    MAP_uDMAChannelControlSet(ulChannel,
                              ulItemSize | ulSrcInc | ulDstInc | ulArbSize);
    
    MAP_uDMAChannelAttributeEnable(ulChannel,UDMA_ATTR_USEBURST);

    MAP_uDMAChannelTransferSet(ulChannel, ulMode,
                               pvSrcBuf, pvDstBuf, ulItemCount);
    
    MAP_uDMAChannelEnable(ulChannel);

}

//*****************************************************************************
//
//! Sets up the Auto Memory transfer
//!
//! \param TBD
//! 
//! This function  
//!        1. Configures the uDMA channel
//!
//! \return None.
//
//*****************************************************************************
void UDMASetupAutoMemTransfer(
                              unsigned long ulChannel,
                              void *pvSrcBuf,
                              void *pvDstBuf,
                              unsigned long size)
{
    SetupTransfer(ulChannel,
                  UDMA_MODE_AUTO,
                  size, 
                  UDMA_SIZE_8, 
                  UDMA_ARB_8,
                  pvSrcBuf, 
                  UDMA_SRC_INC_8, 
                  pvDstBuf, 
                  UDMA_DST_INC_8);
}

//*****************************************************************************
//
//! Sets up the Ping Pong mode of transfer
//!
//! \param TBD
//! 
//! This function  
//!        1. Configures the uDMA channel
//!
//! \return None.
//
//*****************************************************************************
void UDMASetupPingPongTransfer(
                              unsigned long ulChannel,
                              void *pvSrcBuf1,
                              void *pvDstBuf1,
                              void *pvSrcBuf2,
                              void *pvDstBuf2,
                              unsigned long size)
{
    SetupTransfer(ulChannel,
                  UDMA_MODE_PINGPONG,
                  size, 
                  UDMA_SIZE_8, 
                  UDMA_ARB_8,
                  pvSrcBuf1, 
                  UDMA_SRC_INC_8, 
                  pvDstBuf1, 
                  UDMA_DST_INC_8);

    SetupTransfer(ulChannel|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  size, 
                  UDMA_SIZE_8, 
                  UDMA_ARB_8,
                  pvSrcBuf2, 
                  UDMA_SRC_INC_8, 
                  pvDstBuf2, 
                  UDMA_DST_INC_8);
}

//*****************************************************************************
//
//! Start the DMA SW transfer
//!
//! \param None
//! 
//! This function  
//!        1. Starts the SW controlled uDMA transfer
//!
//! \return None.
//
//*****************************************************************************
void UDMAStartSWTransfer(unsigned long ulChannel)
{
    //
    // Request for channel transfer
    //
    MAP_uDMAChannelRequest(ulChannel);
}

//*****************************************************************************
//
//! Stop the DMA transfer
//!
//! \param None
//! 
//! This function  
//!        1. Stops the uDMA transfer on specified channel
//!
//! \return None.
//
//*****************************************************************************
void UDMAStopTransfer(unsigned long ulChannel)
{
    //
    // Disable the channel
    //
    MAP_uDMAChannelDisable(ulChannel);
}


//*****************************************************************************
//
//! De-Initialize the DMA controller
//!
//! \param None
//! 
//! This function  
//!        1. De-Initializes the uDMA module
//!
//! \return None.
//
//*****************************************************************************
void UDMADeInit()
{
    //
    // UnRegister interrupt handlers
    //
    MAP_uDMAIntUnregister(UDMA_INT_SW);
    MAP_uDMAIntUnregister(UDMA_INT_ERR);
    //
    // Disable the uDMA
    //
    MAP_uDMADisable();
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
