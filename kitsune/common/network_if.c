//*****************************************************************************
// network_if.h
//
// Networking interface functions for CC3200 device
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "datatypes.h"
#include "simplelink.h"
#include "protocol.h"
#include "hw_types.h"
#include "uart.h"
#include "timer.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "gpio.h"
#include "pin.h"
#include "utils.h"
#include "network_if.h"
#include "uart_if.h"
#include "timer_if.h"

#define SL_STOP_TIMEOUT                 30

//*****************************************************************************
// Variable related to Connection status
//*****************************************************************************
volatile unsigned short g_usMCUstate = MCU_SLHost_UNINIT;

//*****************************************************************************
// Connection time delay index
//*****************************************************************************
unsigned short g_usConnectIndex;

//*****************************************************************************
// Variables to store TIMER Port,Pin values
//*****************************************************************************
//unsigned int g_uiLED1Port;
//unsigned char g_ucLED1Pin;

//*****************************************************************************
// Variables to store SmartConfig status values
//*****************************************************************************
unsigned short g_uiSmartConfigDone = 0;
unsigned short g_uiSmartConfigStoped = 0;

unsigned char pucCC31xx_Rx_Buffer[CC31xx_APP_BUFFER_SIZE + 
                CC31xx_RX_BUFFER_OVERHEAD_SIZE];
//*****************************************************************************
// Variable used by itoa function 
//*****************************************************************************
const char pcDigits[] = "0123456789";  
//***************************************************************************** 
// Callback function for FreeRTOS 
//*****************************************************************************
#ifdef USE_FREERTOS
//*****************************************************************************
//
//! Application defined hook (or callback) function - the tick hook.
//! The tick interrupt can optionally call this
//!
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void
vApplicationTickHook( void )
{
}

//*****************************************************************************
//
//! Application defined hook (or callback) function - assert
//!
//! \param  File name from which Assert is called
//! 
//! \param  Line number of calling file
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
vApplicationIdleHook( void )
{

}

//*****************************************************************************
//
//! Application provided stack overflow hook function.
//!
//! \param  handle of the offending task
//! \param  name  of the offending task
//! 
//! \return none
//!
//*****************************************************************************
void
vApplicationStackOverflowHook( xTaskHandle *pxTask, signed portCHAR *pcTaskName)
{
  while(1)
  {
    // Infinite loop;
  }
}
#endif

//*****************************************************************************
//!
//! InitCallBack Function. After sl_Start InitCall back function gets triggered
//!
//! \param None
//!
//! \return None
//!
//*****************************************************************************
void
InitCallBack(UINT32 Status)
{
  Network_IF_SetMCUMachineState(MCU_SLHost_INIT);
}


//*****************************************************************************
//
//! This function gets triggered when device acquires IP
//! status to be set. When Device is in DHCP mode recommended to use this.
//!
//! \param pNetAppEvent Pointer indicating device aquired IP
//!
//! \return None
//!
//*****************************************************************************
void sl_NetAppEvtHdlr(SlNetAppEvent_t *pNetAppEvent)
{
    char *pcPtr;
    char cLen;

    switch((pNetAppEvent)->Event)
    {
        case SL_NETAPP_IPV4_ACQUIRED:
        case SL_NETAPP_IPV6_ACQUIRED:

          pcPtr = (char*)&pucCC31xx_Rx_Buffer[0];
          *pcPtr++ = '\f';
          *pcPtr++ = '\r';
          *pcPtr++ = 'I';
          *pcPtr++ = 'P';
          *pcPtr++  = ':';

          cLen = itoa((char)SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3), pcPtr);
          pcPtr += cLen;
          *pcPtr++ = '.';
          cLen = itoa((char)SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2), pcPtr);
          pcPtr += cLen;
          *pcPtr++ = '.';
          cLen = itoa((char)SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1), pcPtr);
          pcPtr += cLen;
          *pcPtr++ = '.';
          cLen = itoa((char)SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0), pcPtr);
          pcPtr += cLen;
          *pcPtr++ = '\f';
          *pcPtr++ = '\r';
          *pcPtr++ = '\0';

          Report((char*)pucCC31xx_Rx_Buffer);
          Network_IF_SetMCUMachineState(MCU_IP_ALLOC);
          break;
       default:
          break;
    }
}

//*****************************************************************************
//
//! An event handler for WLAN connection/disconnection/SmartConfig setup
//! indication
//!
//! \param  pSlWlanEvent is the event passed to the handler
//!
//! \return None
//!
//*****************************************************************************
void sl_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent)
{
    switch(((SlWlanEvent_t*)pSlWlanEvent)->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
            Network_IF_SetMCUMachineState(MCU_AP_ASSOC);
            break;
        case SL_WLAN_DISCONNECT_EVENT:
            Network_IF_UnsetMCUMachineState(MCU_AP_ASSOC);
            break;
        case SL_WLAN_SMART_CONFIG_START_EVENT:
            // SmartConfig operation finished
            g_uiSmartConfigDone = 1;
            g_uiSmartConfigStoped = 1;
            break;
        case SL_WLAN_SMART_CONFIG_STOP_EVENT:
           /* SmartConfig stop operation was completed */
            g_uiSmartConfigDone = 0;
            g_uiSmartConfigStoped = 1;
            break;
    }

}

//*****************************************************************************
//
//! This function gets triggered when HTTP Server receives Application
//! defined GET and POST HTTP Tokens.
//!
//! \param pHttpServerEvent Pointer indicating http server event
//! \param pHttpServerResponse Pointer indicating http server response
//!
//! \return None
//!
//*****************************************************************************
void sl_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse)
{

}
//*****************************************************************************
//
//! Network_IF_InitDriver
//! The function initializes a CC3200 device and triggers it to start operation
//!  
//! \param  None
//!  
//! \return none
//
//*****************************************************************************
void
Network_IF_InitDriver(void)
{
    unsigned char policyVal;

    // Start CC3200 State Machine
    Network_IF_ResetMCUStateMachine();
    //
    // Start the simplelink host
    //
    sl_Start(NULL,NULL,InitCallBack);
    while(!(g_usMCUstate & MCU_SLHost_INIT));
    //reset all policies
    sl_WlanPolicySet(  SL_POLICY_CONNECTION,
                      SL_CONNECTION_POLICY(0,0,0,0,0),
                      &policyVal,
                      1 /*PolicyValLen*/);
    DBG_PRINT("Started SimpleLink Device \n\r");

}

//*****************************************************************************
//
//! Network_IF_DeInitDriver
//! The function de-initializes a CC3200 device
//!  
//! \param  None
//!  
//! \return none
//
//*****************************************************************************
void
Network_IF_DeInitDriver(void)
{
    DBG_PRINT("SL Disconnect...\n\r");
    //
    // Disconnect from the AP
    //
    Network_IF_DisconnectFromAP();
    //
    // Stop the simplelink host
    //
    sl_Stop(SL_STOP_TIMEOUT);
    //
    // Reset the state to uninitialized
    //
    g_usMCUstate = MCU_SLHost_UNINIT;
}


//*****************************************************************************
//
//! Network_IF_ConnectAP  Connect to an Access Point using the specified SSID
//!
//! \param  pcSsid is a string of the AP's SSID
//!
//! \return none
//
//*****************************************************************************
int 
Network_IF_ConnectAP(char *pcSsid, SlSecParams_t SecurityParams)
{
    char acCmdStore[128];
    unsigned short usConnTimeout;
    int iRetVal;
    unsigned long ulIP = 0;
    unsigned long ulSubMask = 0;
    unsigned long ulDefGateway = 0;
    unsigned long ulDns = 0;
    unsigned char ucRecvdAPDetails;

    Network_IF_DisconnectFromAP();
    //
    // This triggers the CC3200 to connect to specific AP
    //
    sl_WlanConnect(pcSsid, strlen(pcSsid), NULL, &SecurityParams, NULL);
    //
    // Wait to check if connection to DIAGNOSTIC_AP succeeds
    //
    while(g_usConnectIndex < 10)
    {
        UtilsDelay(8000000);
        g_usConnectIndex++;
        if(g_usMCUstate & MCU_AP_ASSOC)
        {
            break;
        }
    }
    //
    // Check and loop until async event from network processor indicating Wlan
    // Connect success was received
    //
    while(!(g_usMCUstate & MCU_AP_ASSOC))
    {
        //
        // Disconnect the previous attempt
        //
        //Network_IF_DisconnectFromAP();
        DBG_PRINT("Device could not connect to %s\n\r",pcSsid);

        do
        {
            ucRecvdAPDetails = 0;

            UART_PRINT("\n\r\n\rPlease enter the AP(open) SSID name # ");
            //
            // Get the AP name to connect over the UART
            //
            iRetVal = GetCmd(acCmdStore, sizeof(acCmdStore));
            if(iRetVal > 0)
            {
                //
                // Parse the AP name
                //
                pcSsid = strtok(acCmdStore, ":");
                if(pcSsid != NULL)
                {

                            ucRecvdAPDetails = 1;

                }
            }
        }while(ucRecvdAPDetails == 0);

        DBG_PRINT("\n\rTrying to connect to AP: %s\n\r",pcSsid);
        DBG_PRINT("Key: %s, Len: %d, KeyID: %d, Type: %d\n\r",
                    SecurityParams.Key, SecurityParams.KeyLen
                      , SecurityParams.Type); 
        //
        // Get the current timer tick and setup the timeout accordingly
        //
        usConnTimeout = g_usConnectIndex + 10;
        //
        // This triggers the CC3200 to connect to specific AP
        //
        sl_WlanConnect(pcSsid, strlen(pcSsid), NULL, &SecurityParams, NULL);
        //
        // Wait to check if connection to specifed AP succeeds
        //
        while(g_usConnectIndex < usConnTimeout)
        {
            UtilsDelay(800000);
            g_usConnectIndex++;
        }
    }
	
    //
    // Wait until IP is acquired
    //
    while(!(g_usMCUstate & MCU_IP_ALLOC));
    //
    // Put message on UART
    //
    DBG_PRINT("\n\rDevice has connected to %s\n\r",pcSsid);
    //
    // Get IP address
    //
    Network_IF_IpConfigGet(&ulIP,&ulSubMask,&ulDefGateway,&ulDns);
    //
    // Send the information
    //
    DBG_PRINT("Device IP Address is 0x%x \n\r\n\r", ulIP);
    return 0;
}

//*****************************************************************************
//
//! Disconnect  Disconnects from an Access Point
//!
//! \param  none
//!
//! \return none
//
//*****************************************************************************
void
Network_IF_DisconnectFromAP()
{
    if (g_usMCUstate & MCU_AP_ASSOC)
        sl_WlanDisconnect();

    while((g_usMCUstate & MCU_AP_ASSOC));
}

//*****************************************************************************
//
//! Network_IF_IpConfigGet  Get the IP Address of the device.
//!
//! \param  pulIP IP Address of Device
//! \param  pulSubnetMask Subnetmask of Device
//! \param  pulDefaultGateway Default Gateway value
//! \param  pulDNSServer DNS Server
//!
//! \return none
//
//*****************************************************************************
int 
Network_IF_IpConfigGet(unsigned long *pulIP, unsigned long *pulSubnetMask,
                unsigned long *pulDefaultGateway, unsigned long *pulDNSServer)
{
    unsigned char isDhcp;
    unsigned char len = sizeof(_NetCfgIpV4Args_t);
	_NetCfgIpV4Args_t ipV4 = {0};

	sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&isDhcp,&len,(unsigned char *)&ipV4);
	*pulIP=ipV4.ipV4;
	*pulSubnetMask=ipV4.ipV4Mask;
	*pulDefaultGateway=ipV4.ipV4Gateway;
	*pulDefaultGateway=ipV4.ipV4DnsServer;

	return 1;
}

//*****************************************************************************
//
//! Network_IF_GetHostIP
//!
//! \brief  This function obtains the server IP address using a DNS lookup
//!
//! \param  The server hostname
//!
//! \return IP address: Success or 0: Failure.
//!
//
//*****************************************************************************
unsigned long Network_IF_GetHostIP( char* pcHostName )
{
    int iStatus = 0;
    unsigned long ulDestinationIP;

    iStatus = sl_NetAppDnsGetHostByName(
                        pcHostName, strlen(pcHostName),
                        &ulDestinationIP, SL_AF_INET);
    if (iStatus == 0)
    {
        DBG_PRINT("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d.\n\r\n\r",
              pcHostName,
              SL_IPV4_BYTE(ulDestinationIP,3),
              SL_IPV4_BYTE(ulDestinationIP,2),
              SL_IPV4_BYTE(ulDestinationIP,1),
              SL_IPV4_BYTE(ulDestinationIP,0));
        return ulDestinationIP;
    }
    else
    {
        return 0;
    }
}

//*****************************************************************************
//
//!  \brief  Reset state from the state machine
//!
//!  \param  None
//!
//!  \return none
//!
//*****************************************************************************
void 
Network_IF_ResetMCUStateMachine()
{
    g_usMCUstate = MCU_SLHost_UNINIT;
}
//*****************************************************************************
//
//!  \brief  Return the highest state which we're in
//!
//!  \param  None
//!
//!  \return none
//!
//
//*****************************************************************************
char 
Network_IF_HighestMCUState()
{
    // We start at the highest state and go down, checking if the state is set.
    char cMask = 0x80;
    while(!(g_usMCUstate & cMask))
    {
        cMask = cMask >> 1;
    }
        
    return cMask;
}

//*****************************************************************************
//
//!  \brief  Return the current state bits
//!
//!  \param  None
//!
//!  \return none
//!
//
//*****************************************************************************
char 
Network_IF_CurrentMCUState()
{
    return g_usMCUstate;
}
//*****************************************************************************
//
//!  \brief  sets a state from the state machine
//!
//!  \param  cStat Status of State Machine
//!
//!  \return none
//!
//*****************************************************************************
void 
Network_IF_SetMCUMachineState(char cStat)
{
    char cBitmask = cStat;
    g_usMCUstate |= cBitmask;
}

//*****************************************************************************
//
//!  \brief  Unsets a state from the state machine
//!
//!  \param  cStat Status of State Machine
//!
//!  \return none
//!
//*****************************************************************************
void 
Network_IF_UnsetMCUMachineState(char cStat)
{
    char cBitmask = cStat;
    g_usMCUstate &= ~cBitmask;
    //
    // Set to last element in state list
    //
    int i = FIRST_STATE_LED_NUM;
    //
    // Set all upper bits to 0 as well since state machine cannot have
    // those states.
    //
    while(cBitmask < 0x80)
    {
        g_usMCUstate &= ~cBitmask;
        cBitmask = cBitmask << 1;
        i++;
    }

}

//*****************************************************************************
//
//! itoa
//!
//!    @brief  Convert integer to ASCII in decimal base
//!
//!     @param  cNum is input integer number to convert
//!     @param  cString is output string
//!
//!     @return number of ASCII parameters
//!
//! 
//
//*****************************************************************************
unsigned short itoa(char cNum, char *cString)
{
    char* ptr;
    char uTemp = cNum;
    unsigned short length;

    // value 0 is a special case
    if (cNum == 0)
    {
        length = 1;
        *cString = '0';

        return length;
    }

    // Find out the length of the number, in decimal base
    length = 0;
    while (uTemp > 0)
    {
        uTemp /= 10;
        length++;
    }

    // Do the actual formatting, right to left
    uTemp = cNum;
    ptr = cString + length;
    while (uTemp > 0)
    {
        --ptr;
        *ptr = pcDigits[uTemp % 10];
        uTemp /= 10;
    }

    return length;
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************


