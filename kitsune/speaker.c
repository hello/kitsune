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
unsigned char speaker_data[20*1024]; //16*1024
unsigned char speaker_data_padded[1024]={0}; //16*1024
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


void Speaker1()
{
  FIL		fp;
  WORD		Size;
  FRESULT	res;
  unsigned long totBytesRead = 0;


  long iRetVal = -1;
  res = f_open(&fp, AUDIO_FILE,FA_READ);

  if(res == FR_OK)
  {
    /* Workaround - Read initial 9 bytes that is file name:" mari.rec". We writing file
                      name at start because of hang issue in f_write.
    */
    f_read(&fp, speaker_data, 0, (unsigned short*)&Size);
    UARTprintf("Read : %d Bytes\n\n\r",Size);
    totBytesRead = 0;
  }
  else
  {
	  UARTprintf("Failed to open audio file\n\r");
    LOOP_FOREVER();
  }

  //g_ucSpkrStartFlag = 1;
  unsigned long offset = 0 ;
  while(1)
  {
    //while(g_ucSpkrStartFlag)
    {
      /* Read always in block of 512 Bytes or less else it will stuck in f_read() */
      res = f_read(&fp, speaker_data, 512, (unsigned short*)&Size);
      totBytesRead += Size;
/*
      int i;
  	for (i = 0; i < 512; ++i) {
  		UARTprintf("%x", speaker_data[i]);
  	}
  	*/
      /* Wait to avoid buffer overflow as reading speed is faster than playback */
      while((IsBufferSizeFilled(pRxBuffer,PLAY_WATERMARK) == TRUE)){};

      if(Size>0)
      {
    	  //UARTprintf("Read : %d Bytes totBytesRead: %d\n\n\r",Size, totBytesRead);
    		unsigned int i;
    	  //for(i = 0; i < 512/2; i++){
    		unsigned short *pu16;
    	  pu16 = (unsigned short *)(speaker_data + offset*Size);
    	             	for (i = 0; i < 512/2; i ++) {
    	             		*pu16 = ((*pu16) << 8) | ((*pu16) >> 8);
    	             		pu16++;
    	             	}
    	  //}
			for( i=0;i<512;++i) {
				speaker_data_padded[i*2+1] = speaker_data[i];
				speaker_data_padded[i*2] = speaker_data[i];
			}
			Size *=2;

        iRetVal = FillBuffer(pRxBuffer,(unsigned char*)(speaker_data_padded + offset*Size),\
        		Size);
       // offset = (offset+1)%(sizeof(speaker_data)/512);

        if(iRetVal < 0)
        {
        	UARTprintf("Unable to fill buffer");
          LOOP_FOREVER();
        }
      }
      else
      {
    	//  UARTprintf("Read : %d Bytes totBytesRead: %d\n\n\r",Size, totBytesRead);
        f_close(&fp);
        iRetVal = f_open(&fp,AUDIO_FILE,FA_READ);
        if(iRetVal == FR_OK)
        {
          /* Workaround - Read initial 9 bytes that is file name. We were
                            wrote file name at start because of hang issue
                            in f_write.
          */
          //f_read(&fp, speaker_data, 9, (unsigned short*)&Size);
          //totBytesRead = 9;
        	UARTprintf("speaker task completed\r\n" );
          break;
        }
        else
        {
        	UARTprintf("Failed to open audio file\n\r");
          LOOP_FOREVER();
        }
      }
      if(g_uiPlayWaterMark == 0)
      {
        if(IsBufferSizeFilled(pRxBuffer,PLAY_WATERMARK) == TRUE)
        {
          g_uiPlayWaterMark = 1;
        }
      }
      g_iReceiveCount++;
       UARTprintf("g_iReceiveCount: %d\n\r",g_iReceiveCount);
    }

    MAP_UtilsDelay(1000);

  }
}
