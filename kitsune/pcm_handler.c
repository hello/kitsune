//*****************************************************************************
// pcm_handler.c
//
// PCM Handler Interface
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

//*****************************************************************************
//
//! \addtogroup WifiAudioPlayer
//! @{
//
//*****************************************************************************
/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Hardware & DriverLib library includes. */
#include "hw_types.h"
#include "hw_mcasp.h"
#include "hw_udma.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "debug.h"
#include "interrupt.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "i2s.h"
#include "udma.h"
#include "pin.h"
#include "mcasp_if.h"
#include "circ_buff.h"
#include "pcm_handler.h"
#include "udma_if.h"
#include "i2c_if.h"
#include "uart_if.h"
#include "network.h"

#ifndef TI_CODEC
#include "wolfson.h"
#else
#include "ti_codec.h"
#endif

/* FatFS include */
//#include "ff.h"
//#include "diskio.h"
#include "fatfs_cmd.h"
#include "uartstdio.h"
//*****************************************************************************
//                          LOCAL DEFINES
//*****************************************************************************
#define UDMA_DSTSZ_32_SRCSZ_16          0x21000000
#define mainQUEUE_SIZE		        3
#define UI_BUFFER_EMPTY_THRESHOLD  2048
//*****************************************************************************
//                          GLOBAL VARIABLES
//*****************************************************************************
unsigned short ping[(CB_TRANSFER_SZ)];
unsigned short pong[(CB_TRANSFER_SZ)];
volatile unsigned int guiDMATransferCountTx = 0;
volatile unsigned int guiDMATransferCountRx = 0;
extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

extern TaskHandle_t audio_task_hndl;

//*****************************************************************************
//
//! Callback function implementing ping pong mode DMA transfer
//!
//! \param None
//!
//! This function
//!        1. sets the primary and the secondary buffer for ping pong transfer.
//!		   2. sets the control table.
//!		   3. transfer the appropriate data according to the flags.
//!
//! \return None.
//
//*****************************************************************************
#define swap_endian(x) *(x) = ((*(x)) << 8) | ((*(x)) >> 8);
static volatile unsigned long qqbufsz=0;
void DMAPingPongCompleteAppCB_opt()
{
    unsigned long ulPrimaryIndexTx = 0x4, ulAltIndexTx = 0x24;
    unsigned long ulPrimaryIndexRx = 0x5, ulAltIndexRx = 0x25;
    tDMAControlTable *pControlTable;
    tCircularBuffer *pAudOutBuf = pRxBuffer;
    tCircularBuffer *pAudInBuf = pTxBuffer;
    unsigned char *pucDMADest;
    unsigned char *pucDMASrc;
    unsigned int dma_status;
    unsigned int i2s_status;
    int i = 0;

    traceISR_ENTER();

	i2s_status = I2SIntStatus(I2S_BASE);
	dma_status = uDMAIntStatus();

	if (dma_status & 0x00000010) {
		qqbufsz = GetBufferSize(pAudInBuf);
		HWREG(0x4402609c) = (1 << 10);
		//
		// Get the base address of the control table.
		//
		//pControlTable = (tDMAControlTable *)HWREG(DMA_BASE + UDMA_O_CTLBASE);
		pControlTable = MAP_uDMAControlBaseGet();

		//PRIMARY part of the ping pong
		if ((pControlTable[ulPrimaryIndexTx].ulControl & UDMA_CHCTL_XFERMODE_M)
				== 0) {
			guiDMATransferCountTx += CB_TRANSFER_SZ;
			pucDMADest = (unsigned char *) ping;
			pControlTable[ulPrimaryIndexTx].ulControl |= CTRL_WRD;
			pControlTable[ulPrimaryIndexTx].pvDstEndAddr = (void *) (pucDMADest
					+ END_PTR);
			MAP_uDMAChannelEnable(UDMA_CH4_I2S_RX);

			for (i = 0; i < CB_TRANSFER_SZ/2; i++) {
				pong[i] = pong[2*i+1];
				swap_endian(pong+i);
			}
			FillBuffer(pAudInBuf, (unsigned char*)pong, CB_TRANSFER_SZ);
		} else {
			//ALT part of the ping pong
			if ((pControlTable[ulAltIndexTx].ulControl & UDMA_CHCTL_XFERMODE_M)
					== 0) {
				guiDMATransferCountTx += CB_TRANSFER_SZ;
				pucDMADest = (unsigned char *) pong;
				pControlTable[ulAltIndexTx].ulControl |= CTRL_WRD;
				pControlTable[ulAltIndexTx].pvDstEndAddr = (void *) (pucDMADest
						+ END_PTR);
				MAP_uDMAChannelEnable(UDMA_CH4_I2S_RX);

				for (i = 0; i < CB_TRANSFER_SZ/2; i++) {
					ping[i] = ping[2*i+1];
					swap_endian(ping+i);
				}
				FillBuffer(pAudInBuf, (unsigned char*)ping, CB_TRANSFER_SZ);
			}
		}

		if (guiDMATransferCountTx >= CB_TRANSFER_SZ) {
			signed long xHigherPriorityTaskWoken;

			guiDMATransferCountTx = 0;

			if ( qqbufsz > LISTEN_WATERMARK ) {
				vTaskNotifyGiveFromISR( audio_task_hndl, &xHigherPriorityTaskWoken );
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
	}

	if (dma_status & 0x00000020) {
		qqbufsz = GetBufferSize(pAudOutBuf);
		HWREG(0x4402609c) = (1 << 11);
		pControlTable = MAP_uDMAControlBaseGet();
		if ((pControlTable[ulPrimaryIndexRx].ulControl & UDMA_CHCTL_XFERMODE_M)
				== 0) {
			if ( qqbufsz > CB_TRANSFER_SZ ) {
				guiDMATransferCountRx += CB_TRANSFER_SZ;

				memcpy(  (void*)ping, (void*)GetReadPtr(pAudOutBuf), CB_TRANSFER_SZ);
				UpdateReadPtr(pAudOutBuf, CB_TRANSFER_SZ);

				for (i = CB_TRANSFER_SZ/2-1; i!=-1 ; --i) {
					//the odd ones do not matter
					/*ping[(i<<1)+1] = */ping[i<<1] = ping[i];
				}
			} else {
				memset( ping, 0, sizeof(ping));
			}

			pucDMASrc = (unsigned char*)ping;

			pControlTable[ulPrimaryIndexRx].ulControl |= CTRL_WRD;
			//pControlTable[ulPrimaryIndex].pvSrcEndAddr = (void *)((unsigned long)&gaucZeroBuffer[0] + 15);
			pControlTable[ulPrimaryIndexRx].pvSrcEndAddr =
					(void *) ((unsigned long) pucDMASrc + END_PTR);
			//HWREG(DMA_BASE + UDMA_O_ENASET) = (1 << 5);
			MAP_uDMAChannelEnable(UDMA_CH5_I2S_TX);
		} else {
			if ((pControlTable[ulAltIndexRx].ulControl & UDMA_CHCTL_XFERMODE_M)
					== 0) {
				if ( qqbufsz > CB_TRANSFER_SZ ) {
					guiDMATransferCountRx += CB_TRANSFER_SZ;

					memcpy(  (void*)pong,  (void*)GetReadPtr(pAudOutBuf), CB_TRANSFER_SZ);
					UpdateReadPtr(pAudOutBuf, CB_TRANSFER_SZ);

					for (i = CB_TRANSFER_SZ/2-1; i!=-1 ; --i) {
						//the odd ones do not matter
						/*pong[(i<<1)+1] = */pong[i<<1] = pong[i];
					}
				} else {
					memset( pong, 0, sizeof(pong));
				}

				pucDMASrc = (unsigned char*)pong;

				pControlTable[ulAltIndexRx].ulControl |= CTRL_WRD;
				pControlTable[ulAltIndexRx].pvSrcEndAddr =
						(void *) ((unsigned long) pucDMASrc + END_PTR);
				//HWREG(DMA_BASE + UDMA_O_ENASET) = (1 << 5);
				MAP_uDMAChannelEnable(UDMA_CH5_I2S_TX);
			}
		}

		if (guiDMATransferCountRx >= CB_TRANSFER_SZ) {
			signed long xHigherPriorityTaskWoken;
			qqbufsz = GetBufferSize(pAudOutBuf);
			guiDMATransferCountRx = 0;

			if ( qqbufsz < PLAY_WATERMARK ) {
				vTaskNotifyGiveFromISR( audio_task_hndl, &xHigherPriorityTaskWoken );
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
	}
	uDMAIntClear(dma_status);
	I2SIntClear(I2S_BASE, i2s_status );

	traceISR_EXIT();
}

//*****************************************************************************
//
//! configuring the DMA transfer
//!
//! \param psAudPlayerCtrl is the control structure managing the input data.
//!
//! This function
//!        1. setting the source and the destination for the DMA transfer.
//!		   2. setting the uDMA registers to control actual transfer.
//!
//! \return None.
//
//*****************************************************************************
void SetupPingPongDMATransferTx()
{
    unsigned int * puiTxSrcBuf = AudioCapturerGetDMADataPtr();

    memset(ping, 0, sizeof(ping));
    memset(pong, 0, sizeof(pong));

    // changed to SD card DMA UDMA_CH14_SDHOST_RX
    SetupTransfer(UDMA_CH4_I2S_RX,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)puiTxSrcBuf, 
                  UDMA_CHCTL_SRCINC_NONE,
                  (void *)ping,
                  UDMA_CHCTL_DSTINC_16);
    SetupTransfer(UDMA_CH4_I2S_RX|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)puiTxSrcBuf, 
                  UDMA_CHCTL_SRCINC_NONE,
                  (void *)pong,
                  UDMA_CHCTL_DSTINC_16);

}

void SetupPingPongDMATransferRx()
{
	unsigned int * puiRxDestBuf = AudioRendererGetDMADataPtr();

    memset(ping, 0, sizeof(ping));
    memset(pong, 0, sizeof(pong));

    SetupTransfer(UDMA_CH5_I2S_TX,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)ping,
                  UDMA_CHCTL_SRCINC_16,
                  (void *)puiRxDestBuf, 
                  UDMA_DST_INC_NONE);
    SetupTransfer(UDMA_CH5_I2S_TX|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)pong,
                  UDMA_CHCTL_SRCINC_16,
                  (void *)puiRxDestBuf, 
                  UDMA_DST_INC_NONE);

    
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

