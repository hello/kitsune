//*****************************************************************************
//
//   Copyright (C) 2013 Texas Instruments Incorporated
//
//   All rights reserved. Property of Texas Instruments Incorporated.
//   Restricted rights to use, duplicate or disclose this code are
//   granted through contract.
//
//   The program may not be used without the written permission of
//   Texas Instruments Incorporated or against the terms and conditions
//   stipulated in the agreement under which this program has been supplied,
//   and under no circumstances can it be used with non-TI connectivity device.
//****************************************************************************/
//*****************************************************************************
//
// cc3101.c - 3101z common functions
//
//*****************************************************************************
//*****************************************************************************
//
//! \addtogroup get_weather
//! @{
//
//*****************************************************************************
#include <hw_types.h>
#include <uart.h>
#include <datatypes.h>
#include <simplelink.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <timer.h>
#include <prcm.h>
#include <rom.h>
#include <rom_map.h>
#include <pin.h>
#include <utils.h>
#include <hw_memmap.h>
#include <hw_common_reg.h>
#include <protocol.h>
#include "cc3101.h"
#include "uartstdio.h"

#include "FreeRTOS.h"
#include "semphr.h"

//*****************************************************************************
// Variable related to Connection status
//*****************************************************************************
volatile char g_cc3101state = CC3101_UNINIT;

//*****************************************************************************
// Defining Timer Load Value. Corresponds to 1 second.
//*****************************************************************************
#define PERIODIC_TEST_CYCLES    80000000

#define DBG_PRINT               UARTprintf
#define UART_PRINT              UARTprintf

xSemaphoreHandle cc3101_smphr;

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
    SetCC3101MachineState(CC3101_INIT);
    xSemaphoreGive( cc3101_smphr );
}

//*****************************************************************************
//
//! On Successful completion of Wlan Connect, This function triggers Connection
//! status to be set. 
//!
//! \param  WlanEvent pointer indicating Event type
//!
//! \return None
//!
//*****************************************************************************
void
SimpleLinkWlanEventHandler(void* pArgs)
{
    //
    // Handle the WLAN event appropriately
    //
    switch(((SlWlanEvent_t*)pArgs)->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        SetCC3101MachineState(CC3101_ASSOC);
        xSemaphoreGive( cc3101_smphr );
        break;
            
        case SL_WLAN_DISCONNECT_EVENT:
        UnsetCC3101MachineState(CC3101_ASSOC);
        xSemaphoreGive( cc3101_smphr );
        break;
    }
}

//*****************************************************************************
//
//! This function gets triggered when device acquires IP
//! status to be set. When Device is in DHCP mode recommended to use this.
//!
//! \param NetAppEvent Pointer indicating device aquired IP 
//!
//! \return None
//!
//*****************************************************************************
void
SimpleLinkNetAppEventHandler(void* pArgs)
{
    switch(((SlNetAppEvent_t*)pArgs)->Event)
    {
        case SL_NETAPP_IPV4_ACQUIRED:
        case SL_NETAPP_IPV6_ACQUIRED:
            SetCC3101MachineState(CC3101_IP_ALLOC);
            xSemaphoreGive( cc3101_smphr );
            break;
        default:
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
void
SimpleLinkHttpServerCallback(void *pHttpServerEvent, void *pHttpServerResponse)
{

}
//*****************************************************************************
//
//! initDriver
//! The function initializes a CC3101 device and triggers it to start operation
//!  
//! \param  None
//!  
//! \return none
//
//*****************************************************************************
void
InitDriver(void)
{
    //
    // Start the simplelink host
    //
	vSemaphoreCreateBinary(cc3101_smphr);

    sl_Start(NULL,NULL,InitCallBack);
    if( xSemaphoreTake(cc3101_smphr, 10000 / portTICK_RATE_MS ) != pdPASS
    	|| (g_cc3101state & CC3101_INIT) ) {
        DBG_PRINT("\n\rFailed to start simplelink device\n\r");
    } else {
    	DBG_PRINT("Started SimpleLink Device \n\r");;
    }
}

//*****************************************************************************
//
//! DeInitDriver
//! The function de-initializes a CC3101 device
//!  
//! \param  None
//!  
//! \return none
//
//*****************************************************************************
void
DeInitDriver(void)
{
    DBG_PRINT("SL Disconnect...\n\r");
    //
    // Disconnect from the AP
    //
    sl_WlanDisconnect();
    //
    // Stop the simplelink host
    //
    sl_Stop(0);
    //
    // Reset the state to uninitialized
    //
    g_cc3101state = CC3101_UNINIT;
}

//*****************************************************************************
//
//! Get the Command string from UART
//!
//! \param  pucBuffer is the command store to which command will be populated
//! \param  ucBufLen is the length of buffer store available
//!
//! \return Length of the bytes received. -1 if buffer length exceeded.
//! 
//*****************************************************************************
int
GetCmd(char *pcBuffer, unsigned int uiBufLen)
{
    char cChar;
    int iLen = 0;
    //
    // Wait to receive a character over UART
    //
    cChar = MAP_UARTCharGet(CONSOLE);
    //
    // Echo the received character
    //
    MAP_UARTCharPut(CONSOLE, cChar);
    iLen = 0;
    //
    // Checking the end of Command
    //
    while((cChar != '\r') && (cChar !='\n') )
    {
        //
        // Handling overflow of buffer
        //
        if(iLen >= uiBufLen)
        {
            return -1;
        }
        //
        // Copying Data from UART into a buffer
        //
        if(cChar != '\b')
        { 
            *(pcBuffer + iLen) = cChar;
            iLen++;
        }
        else
        {
            //
            // Deleting last character when you hit backspace 
            //
            if(iLen)
            {
                iLen--;
            }
        }
        //
        // Wait to receive a character over UART
        //
        cChar = MAP_UARTCharGet(CONSOLE);
        //
        // Echo the received character
        //
        MAP_UARTCharPut(CONSOLE, cChar);
    }
    
    *(pcBuffer + iLen) = '\0';
    
    DBG_PRINT("\n\r");

    return iLen;
}

//*****************************************************************************
//
//! ConnectUsingSSID  Connect to an Access Point using the specified SSID
//!
//! \param  pcSsid is a string of the AP's SSID
//!
//! \return none
//
//*****************************************************************************
int 
ConnectUsingSSID(char *acCmd)
{
    unsigned long ulIP = 0;
    unsigned long ulSubMask = 0;
    unsigned long ulDefGateway = 0;
    unsigned long ulDns = 0;
    SlSecParams_t SecurityParams = {0};
    char *pcParse, *pcErrPtr;
    unsigned char ucSecType, ucRecvdAPDetails;
    char *pcSsid;

	ucRecvdAPDetails = 0;

	//
	// Parse the AP name
	//
	pcSsid = strtok(acCmd, ":");

	//
	// Get the security type
	//
	pcParse = strtok(NULL, ":");
	if(pcParse != NULL)
	{
		ucSecType = (unsigned char)strtoul(pcParse,
										   &pcErrPtr, 10);
		//
		// Aligning to the internal definition of security type
		//
		ucSecType -= 1;
		//
		// Update the security parameters as entered by the user
		//
		switch(ucSecType)
		{
		case SL_SEC_TYPE_OPEN:
			SecurityParams.Key = "";
			SecurityParams.KeyLen = 0;
			SecurityParams.Type = SL_SEC_TYPE_OPEN;
			ucRecvdAPDetails = 1;
			break;
		case SL_SEC_TYPE_WEP:
			//
			// Get the password key
			//
			SecurityParams.Key = strtok(NULL, ":");
			if(SecurityParams.Key != NULL)
			{
				//
				// Get the key ID
				//
				pcParse = strtok(NULL, ":");
				if(pcParse != NULL)
				{
				SecurityParams.KeyLen =
					strlen(SecurityParams.Key);
				SecurityParams.Type = SL_SEC_TYPE_WEP;
				ucRecvdAPDetails = 1;
				}
			}
			break;
		case SL_SEC_TYPE_WPA:
			//
			// Get the password key
			//
			SecurityParams.Key = strtok(NULL, ":");
			if(SecurityParams.Key != NULL)
			{
				SecurityParams.KeyLen =
					strlen(SecurityParams.Key);
				SecurityParams.Type = SL_SEC_TYPE_WPA;
				ucRecvdAPDetails = 1;
			}
			break;
		default:
			break;
		}
	}
	if(ucRecvdAPDetails == 0) {
		DBG_PRINT("\n\rfailed to parse: %s\n\r",pcSsid);
		return 1;
	}

	DBG_PRINT("\n\rTrying to connect to AP: %s\n\r",pcSsid);
	DBG_PRINT("Key: %s, Len: %d, KeyID: %d, Type: %d\n\r",
			SecurityParams.Key, SecurityParams.KeyLen, SecurityParams.Type);
	//
	// This triggers the CC3101 to connect to specific AP
	//
	sl_WlanConnect(pcSsid, strlen(pcSsid), NULL, &SecurityParams, NULL);
	//
	// Wait to check if connection to AP succeeds
	//
	if( xSemaphoreTake(cc3101_smphr, 10000 / portTICK_RATE_MS ) != pdPASS
			|| (g_cc3101state & CC3101_ASSOC ) ) {
		DBG_PRINT("\n\rFailed to connect to %s\n\r",pcSsid);
		return 1;
	}

    //
    // Wait until IP is acquired
    if( xSemaphoreTake(cc3101_smphr, 10000 / portTICK_RATE_MS ) != pdPASS
        	|| (g_cc3101state & CC3101_IP_ALLOC) ) {
        DBG_PRINT("\n\rFailed to aquire IP from %s\n\r",pcSsid);
        return 1;
    }

    //
    // Put message on UART
    //
    DBG_PRINT("\n\rDevice has connected to %s\n\r",pcSsid);
    //
    // Get IP address
    //
    IpConfigGet(&ulIP,&ulSubMask,&ulDefGateway,&ulDns);
    //
    // Send the information
    //
    DBG_PRINT("CC3101 Device IP Address is 0x%x \n\r\n\r", ulIP);
    return 0;
}

//*****************************************************************************
//
//! IpConfigGet  Get the IP Address of the device.
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
IpConfigGet(unsigned long *pulIP, unsigned long *pulSubnetMask,
                unsigned long *pulDefaultGateway, unsigned long *pulDNSServer)
{
    unsigned char isDhcp;
    SL_STA_IPV4_ADDR_GET(pulIP,pulSubnetMask,pulDefaultGateway,pulDNSServer,&isDhcp);
    UNUSED(isDhcp);
    return 1;
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
ResetCC3101StateMachine()
{
    g_cc3101state = CC3101_UNINIT;
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
SetCC3101MachineState(char cStat)
{
    char cBitmask = cStat;
    g_cc3101state |= cBitmask;
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
UnsetCC3101MachineState(char cStat)
{
    char cBitmask = cStat;
    g_cc3101state &= ~cBitmask;
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
        g_cc3101state &= ~cBitmask;
        cBitmask = cBitmask << 1;
        i++;
    }

}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************


