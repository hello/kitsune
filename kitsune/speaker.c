//*****************************************************************************
// speaker.c
//
// LINE OUT (Speaker Operation)
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
// Hardware & driverlib library includes
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "hw_ints.h"

// simplelink include

#include "ff.h"


// common interface includes
#include "common.h"

// Demo app includes

#include "circ_buff.h"

#include "integer.h"
#include "uart_if.h"

#include "fatfs_cmd.h"
#include "network.h"
#include "uartstdio.h"

#include "FreeRTOS.h"
#include "task.h"

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern tCircularBuffer *pRxBuffer;

extern int g_iReceiveCount =0;
int g_iRetVal =0;
int iCount =0;
unsigned int g_uiPlayWaterMark = 1;
extern unsigned long  g_ulStatus;
extern unsigned char g_ucSpkrStartFlag;
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************


//*****************************************************************************
//
//! Speaker Routine 
//!
//! \param pvParameters     Parameters to the task's entry function
//!
//! \return None
//
//*****************************************************************************
#include "assert.h"

void Speaker1(char * file)
 {
	FIL fp;
	WORD size;
	FRESULT res;
	unsigned long totBytesRead = 0;

	long iRetVal = -1;
	res = f_open(&fp, file, FA_READ);

	if (res != FR_OK) {
		UARTprintf("Failed to open audio file %d\n\r", res);
		return;
	}
	#define speaker_data_size 256*sizeof(unsigned short)
	unsigned short *speaker_data = pvPortMalloc(speaker_data_size);
	unsigned short *speaker_data_padded = pvPortMalloc(512*sizeof(unsigned short));

	assert(speaker_data);
	assert(speaker_data_padded);

	memset(speaker_data_padded,0,sizeof(speaker_data_padded));

	g_uiPlayWaterMark = 1;
	while (g_ucSpkrStartFlag) {
		/* Read always in block of 512 Bytes or less else it will stuck in f_read() */
		res = f_read(&fp, speaker_data, speaker_data_size, (unsigned short*) &size);
		totBytesRead += size;

		/* Wait to avoid buffer overflow as reading speed is faster than playback */
		while ((IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE)) {
			vTaskDelay(0);
		};

		if (size > 0) {
			unsigned int i;
			for (i = 0; i != (size>>1); ++i) {
				//the odd ones are zeroed already
				speaker_data_padded[i<<1] = speaker_data[i];
			}
			iRetVal = FillBuffer(pRxBuffer, (unsigned char*) (speaker_data_padded), size<<1);

			if (iRetVal < 0) {
				UARTprintf("Unable to fill buffer");
			}
		} else {
			f_close(&fp);
			iRetVal = f_open(&fp, AUDIO_FILE, FA_READ);
			if (iRetVal == FR_OK) {
				g_iReceiveCount = 0;
				UARTprintf("speaker task completed\r\n");
				break;
			} else {
				UARTprintf("Failed to open audio file\n\r");
			}
		}
		if (g_uiPlayWaterMark == 0) {
			if (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
				g_uiPlayWaterMark = 1;
			}
		}
		g_iReceiveCount++;

		if ((g_iReceiveCount % 100) == 0) {
			UARTprintf("g_iReceiveCount: %d\n\r", g_iReceiveCount);
		}
		vTaskDelay(0);
	}
	g_ucSpkrStartFlag = 0;

	vPortFree(speaker_data);
	vPortFree(speaker_data_padded);

}
