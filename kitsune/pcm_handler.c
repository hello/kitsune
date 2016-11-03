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
#include <stdbool.h>

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


/* FatFS include */
//#include "ff.h"
//#include "diskio.h"
#include "fatfs_cmd.h"
#include "uartstdio.h"

#include "codec_debug_config.h"

//*****************************************************************************
//                          LOCAL DEFINES
//*****************************************************************************
// DMA address end-pointer to the last address of the DMA transfer (inclusive).
#define END_PTR (CB_TRANSFER_SZ*4)-4

//*****************************************************************************
//                          GLOBAL VARIABLES
//*****************************************************************************
#if (CODEC_ENABLE_MULTI_CHANNEL==1)
uint16_t ping[(CB_TRANSFER_SZ*2)];
uint16_t pong[(CB_TRANSFER_SZ*2)];
// Ping pong for playback
uint32_t ping_p[(CB_TRANSFER_SZ)];
uint32_t pong_p[(CB_TRANSFER_SZ)];
#else
unsigned short ping[(CB_TRANSFER_SZ)];
unsigned short pong[(CB_TRANSFER_SZ)];
#endif


volatile unsigned int guiDMATransferCountTx = 0;
volatile unsigned int guiDMATransferCountRx = 0;

extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;

extern xSemaphoreHandle record_isr_sem;
extern xSemaphoreHandle playback_isr_sem;;


volatile int _pcm_ping_pong_incoming_stream_mode = PCM_PING_PONG_MODE_SINGLE_CHANNEL_FULL_RATE;

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

static volatile bool can_playback;
volatile int ch = 0;

bool is_playback_active(void){
	return can_playback;
}
void set_isr_playback(bool active){
	can_playback = active;
}

void pcm_set_ping_pong_incoming_stream_mode(const int mode) {
	_pcm_ping_pong_incoming_stream_mode = mode;
}


/* << 20 because I2SIntStatus() shifts DMA_RIS contents by 20.
 */
#define MCASP_WR_DMA_DONE_INT_STS_ACTIVE ((1UL << 11) << 20)
#define MCASP_RD_DMA_DONE_INT_STS_ACTIVE ((1UL << 10) << 20)


/*ramcode*/
void DMAPingPongCompleteAppCB_opt()
{
    unsigned long ulPrimaryIndexTx = 0x4, ulAltIndexTx = 0x24;
    unsigned long ulPrimaryIndexRx = 0x5, ulAltIndexRx = 0x25;
    tDMAControlTable *pControlTable;
    tCircularBuffer *pAudOutBuf = pRxBuffer;
    tCircularBuffer *pAudInBuf = pTxBuffer;
    unsigned char *pucDMADest;
    unsigned char *pucDMASrc;
    unsigned int i2s_status;
    int i = 0;

    traceISR_ENTER();

    /*The interrupt status returns the DMA_RIS, MCASP_XSTAT and MCASP_RSTAT registers.
     *Of the three, only DMA_RIS is used in this ISR callback */
	i2s_status = MAP_I2SIntStatus(I2S_BASE);

	// I2S RX
	if (i2s_status & MCASP_RD_DMA_DONE_INT_STS_ACTIVE) {
		qqbufsz = GetBufferSize(pAudInBuf);
		//
		// Get the base address of the control table.
		//
		//pControlTable = (tDMAControlTable *)HWREG(DMA_BASE + UDMA_O_CTLBASE);
		pControlTable = MAP_uDMAControlBaseGet();

		//PRIMARY part of the ping pong
		if ((pControlTable[ulPrimaryIndexTx].ulControl & UDMA_CHCTL_XFERMODE_M)
				== 0) {
			pucDMADest = (unsigned char *) ping;
			pControlTable[ulPrimaryIndexTx].ulControl |= CTRL_WRD;
			pControlTable[ulPrimaryIndexTx].pvDstEndAddr = (void *) (pucDMADest
					+ END_PTR);
			MAP_uDMAChannelEnable(UDMA_CH4_I2S_RX);

#if (CODEC_ENABLE_MULTI_CHANNEL==0)
			for (i = 0; i < CB_TRANSFER_SZ; i++) {
				swap_endian(pong+i);
			}
#endif

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
			if (_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_SINGLE_CHANNEL_HALF_RATE) {
				int k=0;
				for (i = 0; i< CB_TRANSFER_SZ/2-2 ; i+=2 ) {
					pong[k++] = (pong[i*4+ch]);
				}

				FillBuffer(pAudInBuf, (unsigned char*) pong, 2*k );
			}
			else if (_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_SINGLE_CHANNEL_FULL_RATE) {
				for (i = 0; i< CB_TRANSFER_SZ/2 ; ++i ) {
					pong[i] = pong[i*4+ch];
				}

				FillBuffer(pAudInBuf, (unsigned char*) pong, CB_TRANSFER_SZ );
			}
			else if (_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_ALL_CHANNELS_FULL_RATE) {
				FillBuffer(pAudInBuf, (unsigned char*) pong, sizeof(pong) );
			}
			else if(_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_ALL_CHANNELS_HALF_RATE) {
				int k=0;
				for (i = 0; i< CB_TRANSFER_SZ/8 ; i++, k+=4 ) {
					pong[k+0] = (pong[8*i+0]+pong[8*i+0+4]);
					pong[k+1] = (pong[8*i+1]+pong[8*i+1+4]);
					pong[k+2] = (pong[8*i+2]+pong[8*i+2+4]);
					pong[k+3] = (pong[8*i+3]+pong[8*i+3+4]);
				}

				FillBuffer(pAudInBuf, (unsigned char*) pong, 2*k );
			}
#else
			FillBuffer(pAudInBuf, (unsigned char*)pong, CB_TRANSFER_SZ*2);
#endif
		} else {
			//ALT part of the ping pong
			if ((pControlTable[ulAltIndexTx].ulControl & UDMA_CHCTL_XFERMODE_M)
					== 0) {
#if (CODEC_ENABLE_MULTI_CHANNEL==1)
				guiDMATransferCountTx += CB_TRANSFER_SZ * 4; //bytes
#else
						guiDMATransferCountTx += CB_TRANSFER_SZ*2;
#endif
				pucDMADest = (unsigned char *) pong;
				pControlTable[ulAltIndexTx].ulControl |= CTRL_WRD;
				pControlTable[ulAltIndexTx].pvDstEndAddr = (void *) (pucDMADest
						+ END_PTR);
				MAP_uDMAChannelEnable(UDMA_CH4_I2S_RX);

#if (CODEC_ENABLE_MULTI_CHANNEL==0)
				for (i = 0; i < CB_TRANSFER_SZ; i++) {
					swap_endian(ping+i);
				}
#endif

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
				if (_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_SINGLE_CHANNEL_HALF_RATE) {
					int k=0;
					ping[CB_TRANSFER_SZ/2] = ping[CB_TRANSFER_SZ/2-1];
					for (i = 0; i< CB_TRANSFER_SZ/2-2; i+=2 ) {
						ping[k++] = (ping[i*4+ch]);
					}
					FillBuffer(pAudInBuf, (unsigned char*) ping, 2*k );
				}
				else if (_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_SINGLE_CHANNEL_FULL_RATE) {
					for (i = 0; i< CB_TRANSFER_SZ/2 ; ++i ) {
						ping[i] = ping[i*4+ch];
					}
					FillBuffer(pAudInBuf, (unsigned char*) ping, CB_TRANSFER_SZ );
				}
				else if (_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_ALL_CHANNELS_FULL_RATE) {
					FillBuffer(pAudInBuf, (unsigned char*) ping, sizeof(ping) );
				}
				else if(_pcm_ping_pong_incoming_stream_mode == PCM_PING_PONG_MODE_ALL_CHANNELS_HALF_RATE) {
					int k=0;
					for (i = 0; i< CB_TRANSFER_SZ/8 ; i++, k+=4 ) {
						ping[k+0] = (ping[8*i+0]+ping[8*i+0+4]);
						ping[k+1] = (ping[8*i+1]+ping[8*i+1+4]);
						ping[k+2] = (ping[8*i+2]+ping[8*i+2+4]);
						ping[k+3] = (ping[8*i+3]+ping[8*i+3+4]);
					}

					FillBuffer(pAudInBuf, (unsigned char*) ping, 2*k );
				}
#else
				FillBuffer(pAudInBuf, (unsigned char*)ping, CB_TRANSFER_SZ*2);
#endif
			}
		}

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
		// TODO DKH this if condition is redundant?
		if (guiDMATransferCountTx >= CB_TRANSFER_SZ*4)
#else
		// TODO DKH this if condition is redundant?
		if (guiDMATransferCountTx >= CB_TRANSFER_SZ*2)
#endif
			{
			signed long xHigherPriorityTaskWoken;

			guiDMATransferCountTx = 0;

			if ( qqbufsz > LISTEN_WATERMARK ) {
				xSemaphoreGiveFromISR(record_isr_sem, &xHigherPriorityTaskWoken);
			}
		}
	}

	// I2S TX
	if (i2s_status & MCASP_WR_DMA_DONE_INT_STS_ACTIVE) {
		qqbufsz = GetBufferSize(pAudOutBuf);
		pControlTable = MAP_uDMAControlBaseGet();

		if ((pControlTable[ulPrimaryIndexRx].ulControl & UDMA_CHCTL_XFERMODE_M)
				== 0) {
			if ( qqbufsz > CB_TRANSFER_SZ && can_playback) {
				guiDMATransferCountRx += CB_TRANSFER_SZ*4;

				ReadBuffer(pAudOutBuf,(unsigned char*)ping_p,CB_TRANSFER_SZ);

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
				for (i = CB_TRANSFER_SZ/4-1; i!=-1 ; --i) {
					ping_p[(i<<2) + 3] = 0;
					ping_p[(i<<2) + 2] = (ping_p[i] & 0xFFFF0000);
					ping_p[(i<<2) + 1] = 0;
					ping_p[(i<<2) + 0] = (ping_p[i] & 0xFFFFUL) << 16;
				}
#else
				for (i = CB_TRANSFER_SZ/2-1; i!=-1 ; --i) {
					//the odd ones do not matter
					ping[(i<<1)+1] = 0;
					ping[i<<1] = ping[i];
				}
#endif
			} else {
				memset( ping_p, 0, sizeof(ping_p));
				guiDMATransferCountRx += CB_TRANSFER_SZ*4;
			}
			pucDMASrc = (unsigned char*)ping_p;

			pControlTable[ulPrimaryIndexRx].ulControl |= CTRL_WRD;
			//pControlTable[ulPrimaryIndex].pvSrcEndAddr = (void *)((unsigned long)&gaucZeroBuffer[0] + 15);
			pControlTable[ulPrimaryIndexRx].pvSrcEndAddr =
					(void *) ((unsigned long) pucDMASrc + END_PTR);
			//HWREG(DMA_BASE + UDMA_O_ENASET) = (1 << 5);
			MAP_uDMAChannelEnable(UDMA_CH5_I2S_TX);
		} else {
			if ((pControlTable[ulAltIndexRx].ulControl & UDMA_CHCTL_XFERMODE_M)
					== 0) {
				if ( qqbufsz > CB_TRANSFER_SZ && can_playback) {
					guiDMATransferCountRx += CB_TRANSFER_SZ*4;

					ReadBuffer(pAudOutBuf,(unsigned char*)pong_p,CB_TRANSFER_SZ);

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
				for (i = CB_TRANSFER_SZ/4-1; i!=-1 ; --i) {
					pong_p[(i<<2) + 3] = 0;
					pong_p[(i<<2) + 2] = (pong_p[i] & 0xFFFF0000);
					pong_p[(i<<2) + 1] = 0;
					pong_p[(i<<2) + 0] = (pong_p[i] & 0xFFFFUL) << 16;
				}
#else
					for (i = CB_TRANSFER_SZ/2-1; i!=-1 ; --i) {
						//the odd ones do not matter
						pong[(i<<1)+1] = 0;
						pong[i<<1] = pong[i];
					}
#endif
				} else {
					memset( pong_p, 0, sizeof(pong_p));
					guiDMATransferCountRx += CB_TRANSFER_SZ*4;
				}
				pucDMASrc = (unsigned char*)pong_p;

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
				xSemaphoreGiveFromISR(playback_isr_sem, &xHigherPriorityTaskWoken);
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
		}
	}

	MAP_I2SIntClear(I2S_BASE, i2s_status );

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

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
    UDMASetupTransfer(UDMA_CH4_I2S_RX,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ,
                  UDMA_SIZE_32,
                  UDMA_ARB_8,
                  (void *)puiTxSrcBuf,
                  UDMA_CHCTL_SRCINC_NONE,
                  (void *)ping,
                  UDMA_CHCTL_DSTINC_32);
    UDMASetupTransfer(UDMA_CH4_I2S_RX|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ,
                  UDMA_SIZE_32,
                  UDMA_ARB_8,
                  (void *)puiTxSrcBuf,
                  UDMA_CHCTL_SRCINC_NONE,
                  (void *)pong,
                  UDMA_CHCTL_DSTINC_32);
#else
    // changed to SD card DMA UDMA_CH14_SDHOST_RX
    UDMASetupTransfer(UDMA_CH4_I2S_RX,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)puiTxSrcBuf, 
                  UDMA_CHCTL_SRCINC_NONE,
                  (void *)ping,
                  UDMA_CHCTL_DSTINC_16);
    UDMASetupTransfer(UDMA_CH4_I2S_RX|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)puiTxSrcBuf, 
                  UDMA_CHCTL_SRCINC_NONE,
                  (void *)pong,
                  UDMA_CHCTL_DSTINC_16);
#endif
}

void SetupPingPongDMATransferRx()
{

	unsigned int * puiRxDestBuf = AudioRendererGetDMADataPtr();

    memset(ping_p, 0, sizeof(ping_p));
    memset(pong_p, 0, sizeof(pong_p));

#if (CODEC_ENABLE_MULTI_CHANNEL==1)
    UDMASetupTransfer(UDMA_CH5_I2S_TX,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ,
                  UDMA_SIZE_32,
                  UDMA_ARB_8,
                  (void *)ping_p,
                  UDMA_CHCTL_SRCINC_32,
                  (void *)puiRxDestBuf,
                  UDMA_DST_INC_NONE);
    UDMASetupTransfer(UDMA_CH5_I2S_TX|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ,
                  UDMA_SIZE_32,
                  UDMA_ARB_8,
                  (void *)pong_p,
                  UDMA_CHCTL_SRCINC_32,
                  (void *)puiRxDestBuf,
                  UDMA_DST_INC_NONE);
#else
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ,
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)ping,
                  UDMA_CHCTL_SRCINC_16,
                  (void *)puiRxDestBuf, 
                  UDMA_DST_INC_NONE);
    UDMASetupTransfer(UDMA_CH5_I2S_TX|UDMA_ALT_SELECT,
                  UDMA_MODE_PINGPONG,
                  CB_TRANSFER_SZ, 
                  UDMA_SIZE_16,
                  UDMA_ARB_8,
                  (void *)pong,
                  UDMA_CHCTL_SRCINC_16,
                  (void *)puiRxDestBuf, 
                  UDMA_DST_INC_NONE);
#endif
    
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

