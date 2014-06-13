//*****************************************************************************
// main.h
//
// demonstrates STATION mode on CC3200 device
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

//****************************************************************************
//
//! \addtogroup getting_started_sta
//! @{
//
//****************************************************************************

#include <stdlib.h>
#include <string.h>
#include "hw_types.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "simplelink.h"
#include "protocol.h"
#ifdef ewarm
#include "FreeRTOS.h"
#include "task.h"
#endif
#include "osi.h"
#include "interrupt.h"
#include "pin.h"
#include "prcm.h"
#include "utils.h"
#include "pinmux.h"
#include "gpio_if.h"

#define SSID_NAME               "cc3200demo"
#define SECURITY_TYPE           SL_SEC_TYPE_OPEN 
#define SECURITY_KEY            ""
#define PING_ADDRESS		    "www.ti.com"
#define WEP_KEY_ID              1
#define SL_STOP_TIMEOUT         30    
#define UNUSED(x)   		x = x
#define SPAWN_TASK_PRIORITY     4
#define OSI_STACK_SIZE		2048
#define PING_INTERVAL_TIME 	1000
#define PING_SIZE		20
#define PING_REQ_TIMEOUT	3000
#define PING_NO_OF_ATTEMPT 	3
#define PING_FLAG		0

//*****************************************************************************
// CC3200 Network State Machine Definitions
//*****************************************************************************
enum MCUNetworkStateEnum
{
    MCU_SLHost_UNINIT       = 0x01, // CC3200 NW Driver Uninitialized
    MCU_SLHost_INIT         = 0x02, // CC3200 NW Driver Initialized
    MCU_AP_ASSOC            = 0x04, // CC3200 Associated to AP
    MCU_IP_ALLOC            = 0x08, // CC3200 has IP Address
    MCU_IP_LEASED           = 0x10, // CC3200 Server Initialized
    MCU_SERVER_INIT         = 0x20, // CC3200 Server Initialized
    MCU_CLIENT_CONNECTED    = 0x40, // CC3200 Client Connected to Server
    MCU_PING_COMPLETE       = 0x80  // Ping to AP or server completed
};

//******************************************************************************
//							GLOBAL VARIABLES
//******************************************************************************
unsigned short g_usStatusInfo = 0;
unsigned int g_uiPingPacketsRecv = 0;
unsigned long g_ulDeviceNetworkMode = 0;

//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
void vAssertCalled( const char *pcFile, unsigned long ulLine );
void vApplicationIdleHook();
void sl_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent);
void sl_NetAppEvtHdlr(SlNetAppEvent_t *pNetAppEvent);
void SimpleLinkPingReport(SlPingReport_t *pPingReport);
void WlanConnect();
void InitCallback();
int PingTest(unsigned long ulDefaultGateway);
void WlanStationMode( void *pvParameters );



#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


//*****************************************************************************
//
//! Application defined hook (or callback) function - assert
//!
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void
vAssertCalled( const char *pcFile, unsigned long ulLine )
{
  while(1)
  {
  }
}

//*****************************************************************************
//
//! Application defined idle task hook
//! 
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void
vApplicationIdleHook( void)
{

}

//****************************************************************************
//
//!	\brief This function handles WLAN events
//!
//! \param  pSlWlanEvent is the event passed to the handler
//!
//! \return None
//
//****************************************************************************
void sl_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent)
{
  switch(pSlWlanEvent->Event)
  {
    case SL_WLAN_CONNECT_EVENT:
       g_usStatusInfo |= MCU_AP_ASSOC;
       break;
    case SL_WLAN_DISCONNECT_EVENT:
       g_usStatusInfo &= ~MCU_AP_ASSOC;
       break;
    default:
      break;
  }
}

//****************************************************************************
//
//!	\brief This function handles events for IP address acquisition via DHCP
//!		   indication
//!
//! \param  pNetAppEvent is the event passed to the Handler
//!
//! \return None
//
//****************************************************************************
void sl_NetAppEvtHdlr(SlNetAppEvent_t *pNetAppEvent)
{

  switch( pNetAppEvent->Event )
  {
    case SL_NETAPP_IPV4_ACQUIRED:
	case SL_NETAPP_IPV6_ACQUIRED:
      g_usStatusInfo |= MCU_IP_ALLOC;
      break;
    default:
      break;
  }
}

//****************************************************************************
//
//!	\brief call back function for the ping test
//!
//!	\param  pPingReport is the pointer to the structure containing the result
//!         for the ping test
//!
//!	\return None
//
//****************************************************************************
void SimpleLinkPingReport(SlPingReport_t *pPingReport)
{
  g_usStatusInfo |= MCU_PING_COMPLETE;
  g_uiPingPacketsRecv = pPingReport->PacketsReceived;
}

//*****************************************************************************
//
//! This function gets triggered when HTTP Server receives Application
//! defined GET and POST HTTP Tokens.
//!
//! \param pSlHttpServerEvent Pointer indicating http server event
//! \param pSlHttpServerResponse Pointer indicating http server response
//!
//! \return None
//!
//*****************************************************************************
void sl_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse)
{
}

//****************************************************************************
//
//!	\brief sl_start call back function confirming the simplelink start
//!
//!	\param  None
//!
//!	\return None
//
//****************************************************************************
void InitCallback(UINT32 uiParams)
{
	g_ulDeviceNetworkMode = uiParams;
    g_usStatusInfo |= MCU_SLHost_INIT;
}

//****************************************************************************
//
//! Confgiures the mode in which the device will work
//!
//! \param iMode is the current mode of the device
//!
//! This function
//!    1. Change the mode to STA mode and reset the NWP
//!
//! \return none.
//
//****************************************************************************
void ConfigureMode(int iMode)
{
    if(iMode != ROLE_STA)
    {
    	sl_WlanSetMode(ROLE_STA);
        /* Restart Network processor */
        sl_Stop(SL_STOP_TIMEOUT);
        UtilsDelay(800000);
        sl_Start(NULL,NULL,NULL);
        UtilsDelay(800000);
    }
}

//****************************************************************************
//
//!	\brief pings to the default gateway and ip address of domain "www.ti.com"
//!
//! This function pings to the default gateway to ensure the wlan cannection,
//! then check for the internet connection, if present then get the ip address
//! of Domain name "www.ti.com" and pings to it
//!
//!	\param  ulDefaultGateway is the default gateway for the ap to which the
//!         device is connected
//!
//!	\return -1 for unsuccessful LAN connection, -2 for problem with internet
//!         conection and 0 for succesful ping to the Domain name
//
//****************************************************************************
int PingTest(unsigned long ulDefaultGateway)
{
    int iStatus = 0;
    SlPingStartCommand_t PingParams;
    SlPingReport_t PingReport;
    unsigned long ulIpAddr;
    // Set the ping parameters
    PingParams.PingIntervalTime = PING_INTERVAL_TIME;
    PingParams.PingSize = PING_SIZE;
    PingParams.PingRequestTimeout = PING_REQ_TIMEOUT;
    PingParams.TotalNumberOfAttempts = PING_NO_OF_ATTEMPT;
    PingParams.Flags = PING_FLAG;
    // Fill the GW IP address, which is our AP address
    PingParams.Ip = ulDefaultGateway;
    // Check for LAN connection 
    sl_NetAppPingStart((SlPingStartCommand_t*)&PingParams, SL_AF_INET,
                 (SlPingReport_t*)&PingReport, SimpleLinkPingReport);
    
    while (! (g_usStatusInfo & MCU_PING_COMPLETE))
    {
      //looping till ping is running
    }
    // reset PING Complete state
    g_usStatusInfo &= ~MCU_PING_COMPLETE;

    if (g_uiPingPacketsRecv)
    {
        g_uiPingPacketsRecv = 0;
        // We were able to ping the AP

        /* Check for Internet connection */
        /* Querying for ti.com IP address */
        iStatus = sl_NetAppDnsGetHostByName(PING_ADDRESS, strlen(PING_ADDRESS), &ulIpAddr, SL_AF_INET);
        if (iStatus < 0)
        {
            // LAN connection is successful
            // Problem with Internet connection
            return -2;
        }

        // Replace the ping address to match ti.com IP address
        PingParams.Ip = ulIpAddr;

        // Try to ping www.ti.com
        sl_NetAppPingStart((SlPingStartCommand_t*)&PingParams, SL_AF_INET,
             (SlPingReport_t*)&PingReport, SimpleLinkPingReport);

        while (! (g_usStatusInfo & MCU_PING_COMPLETE))
        {
            //looping till ping is running
        }
      
        if (g_uiPingPacketsRecv)
        {
            // LAN connection is successful
            // Internet connection is successful
            g_uiPingPacketsRecv = 0;
            return 0;
        }
        else
        {
            // LAN connection is successful
            // Problem with Internet connection
            return -2;
        }
    }
    else
    {
      // Problem with LAN connection
      return -1;
    }
}

//****************************************************************************
//
//!	\brief Connecting to a WLAN Accesspoint
//!
//!	This function connects to the required AP (SSID_NAME) with Security
//! parameters specified in te form of macros at the top of this file
//!
//!	\param[in] 			        None
//!
//!	\return	                    None
//!
//!	\warning	If the WLAN connection fails or we don't aquire an IP address,
//!				We will be stuck in this function forever.
//
//****************************************************************************
void WlanConnect()
{
   SlSecParams_t secParams;
   secParams.Key = SECURITY_KEY;
   secParams.KeyLen = strlen(SECURITY_KEY);
   secParams.Type = SECURITY_TYPE;

   sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);

   while ( !(g_usStatusInfo & MCU_AP_ASSOC) || !((g_usStatusInfo & MCU_IP_ALLOC)))
   {
       //looping till ip is acquired
   }
   GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
}

//****************************************************************************
//
//!	\brief start simplelink, connect to the ap and run the ping test
//!
//! This function starts the simplelink, connect to the ap and start the ping
//! test on the default gateway for the ap
//!
//!	\param  pvparameters is the pointer to the list of parameters that can be
//!         passed to the task while creating it
//!
//!	\return None
//
//****************************************************************************
void WlanStationMode( void *pvParameters )
{
    int iTestResult = 0;
    unsigned char ucDHCP = 0;
    sl_Start(NULL,NULL,InitCallback);
    
    while(!(g_usStatusInfo & MCU_SLHost_INIT))
    {
        //looping till simplelink starts
    } 
    ConfigureMode(g_ulDeviceNetworkMode);
    // Connecting to WLAN AP - Set with static parameters defined at the top
        // After this call we will be connected and have IP address */
    WlanConnect();
    
    unsigned char len = sizeof(_NetCfgIpV4Args_t);
	_NetCfgIpV4Args_t ipV4 = {0};

	sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&ucDHCP,&len,(unsigned char *)&ipV4);

    iTestResult = PingTest(ipV4.ipV4Gateway);
    if(iTestResult == 0)
    {
            GPIO_IF_LedOn(MCU_EXECUTE_SUCCESS_IND);
    }
    UNUSED(iTestResult);
    sl_Stop(SL_STOP_TIMEOUT);
    while(1);
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
    //
    // Set vector table base
    //
#if defined(ccs) || defined(gcc)
    IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}


//*****************************************************************************
//							MAIN FUNCTION
//*****************************************************************************
void main()
{
  //
  // Board Initialization
  //
  BoardInit();
  //
  // configure the GPIO pins for LEDs
  //
  PinMuxConfig();
  //
  // Configure all 3 LEDs
  //
  GPIO_IF_LedConfigure(LED1|LED2|LED3);
  //
  // Start the SimpleLink Host
  //
  VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);
  //
  // Start the WlanStationMode task
  //
  osi_TaskCreate( WlanStationMode,
                              (const signed char*)"wireless LAN in station mode",
                              OSI_STACK_SIZE, NULL, 1, NULL );
  //
  // Start the task scheduler
  //
  osi_start();
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
