//*****************************************************************************
// network_if.h
//
// Networking interface macro and function prototypes for CC3200 device
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

#ifndef __NETWORK_IF__H__
#define __NETWORK_IF__H__

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

#include "osi.h"
#include "datatypes.h"

#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
  
#define UNUSED(x) ((x) = (x))
//*****************************************************************************
// GPIO Values
//*****************************************************************************
#ifdef PLATFORM_DVP
#define LED1_GPIO              14
#else
#define LED1_GPIO              9
#endif
  
//*****************************************************************************
// State Machine values 
//*****************************************************************************
#define NUM_STATES 6
#define FIRST_STATE_LED_NUM 1
#define MAX_SSID_LEN        32

#define UART_PRINT              Report
#define DBG_PRINT              	Report
  
#define CC31xx_APP_BUFFER_SIZE	        (5)
#define CC31xx_RX_BUFFER_OVERHEAD_SIZE	(20)

//*****************************************************************************
// CC3200 Network State Machine Definitions
//*****************************************************************************
enum MCUNetworkStateEnum
{
    MCU_SLHost_UNINIT       = 0x0001, // CC3200 NW Driver Uninitialized
    MCU_SLHost_INIT         = 0x0002, // CC3200 NW Driver Initialized
    MCU_AP_ASSOC            = 0x0004, // CC3200 Associated to AP
    MCU_IP_ALLOC            = 0x0008, // CC3200 has IP Address
    MCU_IP_LEASED           = 0x0010, // CC3200 Server Initialized
    MCU_SERVER_INIT         = 0x0020, // CC3200 Server Initialized
    MCU_CLIENT_CONNECTED    = 0x0040, // CC3200 Client Connected to Server
    MCU_PING_COMPLETE       = 0x0080, // Ping to AP or server completed
    MCU_CONNECTION_FAIL     = 0x0100, // Wlan connection failed
    MCU_SMARTCONFIG_START   = 0x0200, // Start SMARTCONFIG
    MCU_SMARTCONFIG_STOP    = 0x0400  // SMART CONFIG Stopped
};

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
#ifdef USE_FREERTOS
extern void vApplicationTickHook( void );
extern void vAssertCalled( const char *pcFile, unsigned long ulLine );
extern void vApplicationIdleHook( void );
extern void vApplicationStackOverflowHook( xTaskHandle *pxTask, 
                                          signed portCHAR *pcTaskName );
#endif
extern void InitCallBack(UINT32 Status);
extern char Network_IF_CurrentMCUState();
extern void sl_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent);
extern void sl_NetAppEvtHdlr(SlNetAppEvent_t *pNetAppEvent);
extern void sl_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse);
extern void Network_IF_InitDriver(void);
extern void Network_IF_DeInitDriver(void);
extern int Network_IF_ConnectAP(char * pcSsid, SlSecParams_t SecurityParams);
extern void Network_IF_DisconnectFromAP();
extern int Network_IF_IpConfigGet(unsigned long *aucIP, unsigned long *aucSubnetMask,
                unsigned long *aucDefaultGateway, unsigned long *aucDNSServer);
extern unsigned long Network_IF_GetHostIP(char* pcHostName);
extern void Network_IF_UnsetMCUMachineState(char stat);
extern void Network_IF_SetMCUMachineState(char stat);
extern void Network_IF_ResetMCUStateMachine();
extern char Network_IF_HighestMCUState();
extern char Network_IF_CurrentMCUState();
extern unsigned short itoa(char cNum, char *cString);
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif //  __MCU_COMMON_H__


