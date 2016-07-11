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
// udma_if.h
//
// uDMA interface MACRO and function prototypes
//
//*****************************************************************************


#ifndef __UDMA_IF_H__
#define __UDMA_IF_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

#define PRINT_DBG               printf

typedef void (*tAppCallbackHndl)(void);
#define MAX_NUM_CH	            64	//32*2 entries
#define CTL_TBL_SIZE	            64	//32*2 entries

#define UDMA_CH5_BITID          (1<<5)

//
// UDMA Interface APIs
//
void UDMAInit();
void UDMAChannelSelect(unsigned int uiChannel, tAppCallbackHndl pfpAppCb);
void UDMASetupAutoMemTransfer(
                              unsigned long ulChannel,
                              void *pvSrcBuf,
                              void *pvDstBuf,
                              unsigned long size);
void UDMASetupPingPongTransfer(
                              unsigned long ulChannel,
                              void *pvSrcBuf1,
                              void *pvDstBuf1,
                              void *pvSrcBuf2,
                              void *pvDstBuf2,
                              unsigned long size);
void UDMAStartTransfer(unsigned long ulChannel);
void UDMAStopTransfer(unsigned long ulChannel);
void UDMADeInit();
void UDMASetupTransfer(unsigned long ulChannel, unsigned long ulMode,
                       unsigned long ulItemCount, unsigned long ulItemSize,
                       unsigned long ulArbSize, void *pvSrcBuf,
                       unsigned long ulSrcInc, void *pvDstBuf, unsigned long ulDstInc);
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __UDMA_IF_H__

