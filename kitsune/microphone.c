//*****************************************************************************
// Microphone.c
//
// Line IN (Microphone interface)
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
#include "network.h"
#include "circ_buff.h"
#include "simplelink.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "hw_ints.h"
// common interface includes
#include "common.h"
#include "hw_memmap.h"
#include "fatfs_cmd.h"
#include "ff.h"
//******************************************************************************
//							GLOBAL VARIABLES
//******************************************************************************

extern unsigned int clientIP;
extern tCircularBuffer *pTxBuffer;
extern tUDPSocket g_UdpSock;
extern int g_iSentCount =0;
unsigned long g_ulConnected = 0; 

extern tCircularBuffer *pRxBuffer;
extern int g_iReceiveCount;
extern unsigned int g_uiPlayWaterMark;
//extern unsigned int g_uiIpObtained;
extern volatile unsigned char g_ucMicStartFlag;
extern unsigned char speaker_data[16*1024];

#ifdef MULTICAST
//*****************************************************************************
//
//! Send Multicast Packet 
//!
//! \param none
//!
//! \return None
//
//*****************************************************************************
void SendMulticastPacket()
{
    int uiPort = 5050;
    struct sockaddr_in stClient;
    stClient.sin_family = AF_INET;
    stClient.sin_addr.s_addr = htonl(ADDR_MULTICAST_GROUP);
    stClient.sin_port = htons(uiPort);
  
    sendto(g_UdpSock.iSockDesc, (char*)(pTxBuffer->pucReadPtr),PACKET_SIZE,
        0,(struct sockaddr*)&(stClient),sizeof(stClient));	
}
#endif

//*****************************************************************************
//
//! Microphone Routine 
//!
//! \param pvParameters     Parameters to the task's entry function
//!
//! \return None
//
//*****************************************************************************

void Microphone1()
{
#ifdef NETWORK
#ifdef MULTICAST
    //Wait for Network Connection
    while(g_uiIpObtained == 0)
    {
        
    }
#else
#if 0
    //Wait for Network Connection and Speaker Discovery
    while(g_uiIpObtained == 0 || g_UdpSock.Client.sin_addr.s_addr==INVALID_CLIENT_ADDRESS)
    {
        
    }
#endif
#endif //MULTICAST
#endif //NETWORK

    // Open the file for reading.

    const char* file_name = "/POD101";

    FIL file_obj;
    FILINFO file_info;
    memset(&file_obj, 0, sizeof(file_obj));
    FIL* file_ptr = &file_obj;

    FRESULT res = f_open(&file_obj, file_name, FA_WRITE|FA_OPEN_ALWAYS);
    //UARTprintf("res :%d\n",res);


    if(res != FR_OK && res != FR_EXIST){
    	UARTprintf("File open %s failed: %d\n", file_name, res);
    	return -1;
    }


    memset(&file_info, 0, sizeof(file_info));

    f_stat(file_name, &file_info);

    if( file_info.fsize != 0 ){
    	res = f_lseek(&file_obj, file_info.fsize);
    }

    while(1)
    {     
        //while(g_ucMicStartFlag)
        {
            int iBufferFilled = 0;

            iBufferFilled = GetBufferSize(pTxBuffer);
            //UARTprintf("iBufferFilled %d\r\n" , iBufferFilled);
            if(iBufferFilled >= (2*PACKET_SIZE))
            {
#if 0
            int i;
            long tmp = 0;
            for(i = 0; i < pTxBuffer->ulBufferSize; i++){
            	tmp += pTxBuffer->pucBufferStartPtr[i];
            }
            UARTprintf("loop into iBufferFilled pRxBuffer %x\n", tmp);
            //long FIFO_DMA;
            //FIFO_DMA = I2STxFIFOStatusGet(I2S_BASE);
            //UARTprintf("FIFO_DMA %x\n", FIFO_DMA);
            //vTaskDelay(1000);
#endif

#ifdef NETWORK

#ifndef MULTICAST
#if 0
                sendto(g_UdpSock.iSockDesc, (char*)(pTxBuffer->pucReadPtr),PACKET_SIZE,
                0,(struct sockaddr*)&(g_UdpSock.Client),sizeof(g_UdpSock.Client));
#endif
#else      //MULTICAST         
                SendMulticastPacket();
#endif     //MULTICAST      
#else   //NETWORK       
                FillBuffer(pRxBuffer,(unsigned char*)(pTxBuffer->pucReadPtr), PACKET_SIZE);
                if(g_uiPlayWaterMark == 0)
                {
                    if(IsBufferSizeFilled(pRxBuffer,PLAY_WATERMARK) == TRUE)
                    {                    
                        g_uiPlayWaterMark = 1;
                    }
                }      
                g_iReceiveCount++;

#endif   //NETWORK       

             	if(g_iSentCount == 6250){
             		 res = f_close(file_ptr);
             		UARTprintf("mic task completed\r\n" );
             		g_iSentCount = 0;
             		break;
             	}
                 //  f_append("/Pud",*(pTxBuffer->pucReadPtr),512);

        	    WORD bytes = 0;
        	    WORD bytes_written = 0;
        	    WORD bytes_to_write = PACKET_SIZE;
                 do {


                	 FRESULT res= f_write(file_ptr, (pTxBuffer->pucReadPtr)+bytes_written , bytes_to_write-bytes_written, &bytes );
                	 //UARTprintf("res is %d\n ",res);
                 		bytes_written+=bytes;
                 		if (res != FR_OK)
                 		{
                 			UARTprintf("Write fail %d\n",res);
                 			break;
                 		}

                 	} while( bytes_written < bytes_to_write );

                 UpdateReadPtr(pTxBuffer, PACKET_SIZE);
                 UARTprintf("pTxBuffer %x\n\r", *(pTxBuffer->pucReadPtr));
                 g_iSentCount++;
            }

        }      
    MAP_UtilsDelay(1000);
    }
}
