//*****************************************************************************
// network.h
//
// Network Interface
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

#ifndef __NETWORK_H__
#define __NETWORK_H__

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
  
#include "simplelink.h"

#define INVALID_CLIENT_ADDRESS            0x00000000 //Invalid clientIP 
//*****************************************************************************
//
// MultiCast Group IP Address (Any IP in Range 224.0.0.0 - 239.255.255.255)
//
//*****************************************************************************
#define ADDR_MULTICAST_GROUP 	   0xE00000FB   //"224.0.0.251" //Using mDNS IP
  
//*****************************************************************************
//
// UDP Socket Port
//
//*****************************************************************************
#define AUDIO_PORT                     (5050)  
//*****************************************************************************
//
// Define Packet Size, Rx and Tx Buffer
//
//*****************************************************************************
#define PING_PONG_CHUNK_SIZE (2048)

#define RX_BUFFER_SIZE          (4*PING_PONG_CHUNK_SIZE)
#define PLAY_WATERMARK          (RX_BUFFER_SIZE/2)

#define TX_BUFFER_SIZE          (4*PING_PONG_CHUNK_SIZE)

extern void * audio_dma_sem;


#define AUDIO_FILE "DIGIAUX.raw" //armi.rec
//*****************************************************************************
//
// Define Audio Input/Output Connection
//
//*****************************************************************************    
#define MIC 1
#define SPEAKER 1
 
//*****************************************************************************
//
// Define NETWORK for Streaming in Network
// 0 - LoopBack, MIC is Connected to Speaker
//
//*****************************************************************************    
#define NETWORK 1

  //*****************************************************************************
//
// UDP Socket Structure
//
//***************************************************************************** 
typedef struct UDPSocket
{
    int iSockDesc; //Socket FD
    struct sockaddr_in Server; //Address to Bind - INADDR_ANY for Local IP
    struct sockaddr_in Client; //Client Address or Multicast Group
    int iServerLength;
    int iClientLength;
}tUDPSocket;

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
void ConnectToNetwork();

void CreateUdpServer(tUDPSocket *pSock);

void SmartConfigConnect();

void ReceiveMulticastPacket();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //__NETWORK_H__
