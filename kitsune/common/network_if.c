/*
 *   Copyright (C) 2015 Texas Instruments Incorporated
 *
 *   All rights reserved. Property of Texas Instruments Incorporated.
 *   Restricted rights to use, duplicate or disclose this code are
 *   granted through contract.
 *
 *   The program may not be used without the written permission of
 *   Texas Instruments Incorporated or against the terms and conditions
 *   stipulated in the agreement under which this program has been supplied,
 *   and under no circumstances can it be used with non-TI connectivity device.
 *   
 */

//*****************************************************************************
// network_if.c
//
// Networking interface functions for CC3220 device
//
//*****************************************************************************

#include <string.h>
#include <stdlib.h>

// Simplelink includes 
#include "simplelink.h"

// driverlib includes 
#include "hw_types.h"
#include <driverlib/rom_map.h>
#include <driverlib/rom.h>
#include <driverlib/utils.h>

// free-rtos/TI-rtos include
#ifdef SL_PLATFORM_MULTI_THREADED
#include "osi.h"
#endif

// common interface includes 
#include "network_if.h"

#ifndef NOTERM
#include "uart_if.h"
#endif
#include "timer_if.h"
#include "common.h"


// Network App specific status/error codes which are used only in this file
typedef enum{
     // Choosing this number to avoid overlap w/ host-driver's error codes
    DEVICE_NOT_IN_STATION_MODE = -0x7F0,
    DEVICE_NOT_IN_AP_MODE = DEVICE_NOT_IN_STATION_MODE - 1,
    DEVICE_NOT_IN_P2P_MODE = DEVICE_NOT_IN_AP_MODE - 1,

    STATUS_CODE_MAX = -0xBB8
}e_NetAppStatusCodes;

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long  g_ulStatus = 0;   /* SimpleLink Status */
unsigned long  g_ulStaIp = 0;    /* Station IP address */
unsigned long  g_ulGatewayIP = 0; /* Network Gateway IP address */
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; /* Connection SSID */
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; /* Connection BSSID */
volatile unsigned short g_usConnectIndex; /* Connection time delay index */
const char     pcDigits[] = "0123456789"; /* variable used by itoa function */
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************




#ifdef USE_FREERTOS
//*****************************************************************************
// FreeRTOS User Hook Functions enabled in FreeRTOSConfig.h
//*****************************************************************************

//*****************************************************************************
//!
//! \brief Application defined hook (or callback) function - assert
//!
//! \param[in]  pcFile - Pointer to the File Name
//! \param[in]  ulLine - Line Number
//! 
//! \return none
//!
//*****************************************************************************
void
vAssertCalled( const char *pcFile, unsigned long ulLine )
{
    //Handle Assert here
    while(1)
    {
    }
}

//*****************************************************************************
//!
//! \brief Application defined idle task hook
//! 
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void
vApplicationIdleHook( void)
{
    //Handle Idle Hook for Profiling, Power Management etc
}

//*****************************************************************************
//!
//! \brief Application defined malloc failed hook
//! 
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void vApplicationMallocFailedHook()
{
    //Handle Memory Allocation Errors
    while(1)
    {
    }
}

//*****************************************************************************
//!
//! \brief Application defined stack overflow hook
//! 
//! \param  none
//! 
//! \return none
//!
//*****************************************************************************
void vApplicationStackOverflowHook( OsiTaskHandle *pxTask,
                                   signed char *pcTaskName)
{
    //Handle FreeRTOS Stack Overflow
    while(1)
    {
    }
}
#endif //USE_FREERTOS


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//!
//! \brief 	On Successful completion of Wlan Connect, This function triggers
//! 		connection status to be set.
//!
//! \param  pSlWlanEvent pointer indicating Event type
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pSlWlanEvent)
{
    switch(pSlWlanEvent->Id)
    {
        case SL_WLAN_EVENT_CONNECT:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'-Applications
            // can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //
            //
            memcpy(g_ucConnectionSSID,pSlWlanEvent->Data.Connect.SsidName,
                               pSlWlanEvent->Data.Connect.SsidLen);
            memcpy(g_ucConnectionBSSID,
                               pSlWlanEvent->Data.Connect.Bssid,
                               SL_WLAN_BSSID_LENGTH);
            UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , BSSID: "
                                    "%x:%x:%x:%x:%x:%x\n\r", g_ucConnectionSSID,
                                    g_ucConnectionBSSID[0], g_ucConnectionBSSID[1],
                                    g_ucConnectionBSSID[2], g_ucConnectionBSSID[3],
                                    g_ucConnectionBSSID[4], g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_EVENT_DISCONNECT:
        {
        	SlWlanEventDisconnect_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_ACQUIRED);

            pEventData = &pSlWlanEvent->Data.Disconnect;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED
            if(SL_WLAN_DISCONNECT_USER_INITIATED == pEventData->ReasonCode)
            {
                UART_PRINT("Device disconnected from the AP on application's "
                            "request \n\r");
            }
            else
            {
                UART_PRINT("Device disconnected from the AP on an ERROR..!! \n\r");
            }

        }
        break;

        case SL_WLAN_EVENT_STA_ADDED:
        {
            // when device is in AP mode and any client connects to device cc3xxx
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected client (like SSID, MAC etc) will be
            // available in 'slPeerInfoAsyncResponse_t' - Applications
            // can use it if required
            //
            // slPeerInfoAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.APModeStaConnected;
            //

        }
        break;

        case SL_WLAN_EVENT_STA_REMOVED:
        {
            // when client disconnects from device (AP)
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

            //
            // Information about the connected client (like SSID, MAC etc) will
            // be available in 'slPeerInfoAsyncResponse_t' - Applications
            // can use it if required
            //
            // slPeerInfoAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.APModestaDisconnected;
            //            
        }
        break;

        default:
        {
            UART_PRINT("[WLAN EVENT] Unexpected event %d\n\r", pSlWlanEvent->Id);
        }
        break;
    }
}


//*****************************************************************************
//!
//! \brief		The Function Handles the Fatal errors
//!
//! \param[in]  pFatalErrorEvent - Contains the fatal error data
//!
//! \return 	None
//!
//*****************************************************************************
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
	switch (slFatalErrorEvent->Id)
	{
		case SL_DEVICE_EVENT_FATAL_DEVICE_ABORT:
		{
			UART_PRINT("[ERROR] - FATAL ERROR: Abort NWP event detected: AbortType=%d, AbortData=0x%x\n\r",slFatalErrorEvent->Data.DeviceAssert.Code,slFatalErrorEvent->Data.DeviceAssert.Value);
			while(1);
		}

		case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
		{
			UART_PRINT("[ERROR] - FATAL ERROR: Driver Abort detected. \n\r");
			while(1);
		}

		case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
		{
			UART_PRINT("[ERROR] - FATAL ERROR: No Cmd Ack detected [cmd opcode = 0x%x] \n\r", slFatalErrorEvent->Data.NoCmdAck.Code);
			while(1);
		}

		case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
		{
			UART_PRINT("[ERROR] - FATAL ERROR: Sync loss detected n\r");
		}
		break;

		case SL_DEVICE_EVENT_FATAL_CMD_TIMEOUT:
		{
			UART_PRINT("[ERROR] - FATAL ERROR: Async event timeout detected [event opcode =0x%x]  \n\r", slFatalErrorEvent->Data.CmdTimeout.Code);
		}
		break;

	default:
		UART_PRINT("[ERROR] - FATAL ERROR: Unspecified error detected \n\r");
			break;
	}
}

//*****************************************************************************
//!
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    switch(pNetAppEvent->Id)
    {
        case SL_NETAPP_EVENT_IPV4_ACQUIRED:
        case SL_NETAPP_EVENT_IPV6_ACQUIRED:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_ACQUIRED);
            UART_PRINT("[NETAPP EVENT] IP acquired by the device\n\r");
        }
        break;
        
        case SL_NETAPP_EVENT_DHCPV4_LEASED:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
        
            g_ulStaIp = (pNetAppEvent)->Data.IpLeased.IpAddress;
            
            UART_PRINT("[NETAPP EVENT] IP Leased to Client: IP=%d.%d.%d.%d , ",
                        SL_IPV4_BYTE(g_ulStaIp,3), SL_IPV4_BYTE(g_ulStaIp,2),
                        SL_IPV4_BYTE(g_ulStaIp,1), SL_IPV4_BYTE(g_ulStaIp,0));
        }
        break;
        
        case SL_NETAPP_EVENT_DHCPV4_RELEASED:
        {
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

            UART_PRINT("[NETAPP EVENT] IP Released for Client: IP=%d.%d.%d.%d , ",
                        SL_IPV4_BYTE(g_ulStaIp,3), SL_IPV4_BYTE(g_ulStaIp,2),
                        SL_IPV4_BYTE(g_ulStaIp,1), SL_IPV4_BYTE(g_ulStaIp,0));

        }
        break;

        default:
        {
            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                       pNetAppEvent->Id);
        }
        break;
    }
}


//*****************************************************************************
//!
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlNetAppHttpServerEvent_t *pHttpEvent,
                                  SlNetAppHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
}

//*****************************************************************************
//!
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
               pDevEvent->Data.Error.Code,
               pDevEvent->Data.Error.Source);
}


//*****************************************************************************
//!
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    //
    // This application doesn't work w/ socket - Events are not expected
    //
    switch( pSock->Event )
    {
        case SL_SOCKET_TX_FAILED_EVENT:
            switch( pSock->SocketAsyncEvent.SockTxFailData.Status)
            {
                case SL_ERROR_BSD_ECLOSE:
                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                                "failed to transmit all queued packets\n\n", 
                                    pSock->SocketAsyncEvent.SockTxFailData.Sd);
                    break;
                default: 
                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                                "(%d) \n\n",
                                pSock->SocketAsyncEvent.SockTxFailData.Sd, pSock->SocketAsyncEvent.SockTxFailData.Status);
                  break;
            }
            break;

        default:
        	UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
          break;
    }

}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************


//****************************************************************************
//!
//!    \brief This function initializes the application variables
//!
//!    \param[in]  None
//!
//!    \return     0 on success, negative error-code on error
//
//****************************************************************************
void InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulStaIp = 0;
    g_ulGatewayIP = 0;
    memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
	SlDeviceVersion_t   ver = {0};
    SlWlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucConfigOpt = 0;
    unsigned short ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(lMode);

    if (ROLE_AP != lMode)
    {
    	lMode = sl_WlanSetMode(ROLE_AP);
    	ASSERT_ON_ERROR(lMode);

    	lRetVal = sl_Stop(0xFF);
    	ASSERT_ON_ERROR(lRetVal);

    	lRetVal = sl_Start(0, 0, 0);
    	ASSERT_ON_ERROR(lRetVal);

        if (ROLE_AP != lRetVal)
        {
        	return -1;
        }
    }
    
    // Get the device's version-information
    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
    ucConfigLen = sizeof(ver);
    lRetVal = sl_DeviceGet(SL_DEVICE_GENERAL, &ucConfigOpt,
                                     &ucConfigLen, (unsigned char *)(&ver));
	ASSERT_ON_ERROR(lRetVal);
    
    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
    ver.FwVersion[0],ver.FwVersion[1],
    ver.FwVersion[2],ver.FwVersion[3],
    ver.PhyVersion[0],ver.PhyVersion[1],
    ver.PhyVersion[2],ver.PhyVersion[3]);

    // Set connection policy to Auto + Auto Provisisoning 
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION,
                                SL_WLAN_CONNECTION_POLICY(1, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(lRetVal);

    

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal)
    {
        // Wait
        while(IS_CONNECTED(g_ulStatus))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
              _SlNonOsMainLoopTask(); 
#endif
        }
    }


    // Disable scan
    ucConfigOpt = SL_WLAN_SCAN_POLICY(0, 0);
    lRetVal = sl_WlanPolicySet(SL_WLAN_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, 
    		SL_WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_WLAN_POLICY_PM , SL_WLAN_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Unregister mDNS services
    lRetVal = sl_NetAppMDNSUnRegisterService(0, 0, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterBitmap, 0xFF, 8);
    lRetVal = sl_WlanSet(SL_WLAN_RX_FILTERS_ID, SL_WLAN_RX_FILTER_REMOVE,
    		sizeof(SlWlanRxFilterOperationCommandBuff_t),
			(_u8 *)&RxFilterIdMask);

    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(lRetVal);

    InitializeAppVariables();
    
    return lRetVal; // Success
}


//*****************************************************************************
//
//! Network_IF_InitDriver
//! The function initializes a CC3220 device and triggers it to start operation
//!  
//! \param  uiMode (device mode in which device will be configured)
//!  
//! \return 0 : sucess, -ve : failure
//
//*****************************************************************************
long
Network_IF_InitDriver(unsigned int uiMode)
{
    long lRetVal = -1;
    // Reset CC3220 Network State Machine
    InitializeAppVariables();

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of application
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
           UART_PRINT("Failed to configure the device in its default state \n\r");

        LOOP_FOREVER();
    }

    UART_PRINT("Device is configured in default state \n\r");

    //
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //

    lRetVal = sl_Start(NULL,NULL,NULL);

    if (lRetVal < 0)
    {
        UART_PRINT("Failed to start the device \n\r");
        LOOP_FOREVER();
    }
    
    switch(lRetVal)
    {
    case ROLE_STA:
        UART_PRINT("Device came up in Station mode\n\r");
        break;
    case ROLE_AP:
        UART_PRINT("Device came up in Access-Point mode\n\r");
        break;
    case ROLE_P2P:
        UART_PRINT("Device came up in P2P mode\n\r");
        break;
    default:
        UART_PRINT("Error:unknown mode\n\r");
        break;
    }
    
    if(uiMode != lRetVal)
    {      
        UART_PRINT("Switching Networking mode on application request\n\r");
        // Switch to AP role and restart
        lRetVal = sl_WlanSetMode(uiMode);
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(0xFF);

        lRetVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);
        
        if(lRetVal == uiMode)
        {
            switch(lRetVal)
            {
            case ROLE_STA:
                UART_PRINT("Device came up in Station mode\n\r");
                break;
            case ROLE_AP:
                UART_PRINT("Device came up in Access-Point mode\n\r");
                // If the device is in AP mode, we need to wait for this event
                // before doing anything
                while(!IS_IP_ACQUIRED(g_ulStatus))
                {
#ifndef SL_PLATFORM_MULTI_THREADED
                    _SlNonOsMainLoopTask();
#else
                    osi_Sleep(1);
#endif
                }
                break;
            case ROLE_P2P:
                UART_PRINT("Device came up in P2P mode\n\r");
                break;
            default:
                UART_PRINT("Error:unknown mode\n\r");
                break;
            }
        }
        else
        {
            UART_PRINT("could not configure correct netowrking mode\n\r");
            LOOP_FOREVER();
        }
    }
    else
    {
        if(lRetVal == ROLE_AP)
        {
            while(!IS_IP_ACQUIRED(g_ulStatus))
            {
#ifndef SL_PLATFORM_MULTI_THREADED
                _SlNonOsMainLoopTask();
#else
                osi_Sleep(1);
#endif
            }
        }
    }
    return 0;
}

//*****************************************************************************
//
//! Network_IF_DeInitDriver
//! The function de-initializes a CC3220 device
//!  
//! \param  None
//!  
//! \return On success, zero is returned. On error, other
//
//*****************************************************************************
long
Network_IF_DeInitDriver(void)
{
    long lRetVal = -1;
    UART_PRINT("SL Disconnect...\n\r");

    //
    // Disconnect from the AP
    //
    lRetVal = Network_IF_DisconnectFromAP();

    //
    // Stop the simplelink host
    //
    sl_Stop(SL_STOP_TIMEOUT);

    //
    // Reset the state to uninitialized
    //
    Network_IF_ResetMCUStateMachine();
    return lRetVal;
}


//*****************************************************************************
//
//! Network_IF_ConnectAP  Connect to an Access Point using the specified SSID
//!
//! \param[in]  pcSsid is a string of the AP's SSID
//! \param[in]  SecurityParams is Security parameter for AP
//!
//! \return On success, zero is returned. On error, -ve value is returned
//
//*****************************************************************************
long
Network_IF_ConnectAP(char *pcSsid, SlWlanSecParams_t SecurityParams)
{
#ifndef NOTERM  
    char acCmdStore[128];
    unsigned short usConnTimeout;
    unsigned char ucRecvdAPDetails;
#endif
    long lRetVal;
    unsigned long ulIP = 0;
    unsigned long ulSubMask = 0;
    unsigned long ulDefGateway = 0;
    unsigned long ulDns = 0;

    //
    // Disconnect from the AP
    //
    Network_IF_DisconnectFromAP();
    
    CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
    CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_ACQUIRED);
    //
    // This triggers the CC3220 to connect to specific AP
    //
    lRetVal = sl_WlanConnect((signed char *)pcSsid, strlen((const char *)pcSsid),
                        NULL, &SecurityParams, NULL);
    ASSERT_ON_ERROR(lRetVal);

    //
    // Wait for ~10 sec to check if connection to desire AP succeeds
    //
    while(g_usConnectIndex < 15)
    {
#ifndef SL_PLATFORM_MULTI_THREADED
        _SlNonOsMainLoopTask();
#else
              osi_Sleep(1);
#endif
        ROM_UtilsDelayDirect(16000000);
        if(IS_CONNECTED(g_ulStatus) && IS_IP_ACQUIRED(g_ulStatus))
        {
            break;
        }
        g_usConnectIndex++;
    }

#ifndef NOTERM
    //
    // Check and loop until AP connection successful, else ask new AP SSID name
    //
    while(!(IS_CONNECTED(g_ulStatus)) || !(IS_IP_ACQUIRED(g_ulStatus)))
    {
        //
        // Disconnect the previous attempt
        //
        Network_IF_DisconnectFromAP();

        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_ACQUIRED);
        UART_PRINT("Device could not connect to %s\n\r",pcSsid);

        do
        {
            ucRecvdAPDetails = 0;

            UART_PRINT("\n\r\n\rPlease enter the AP(open) SSID name # ");

            //
            // Get the AP name to connect over the UART
            //
            lRetVal = GetCmd(acCmdStore, sizeof(acCmdStore));
            if(lRetVal > 0)
            {
                // remove start/end spaces if any
                lRetVal = TrimSpace(acCmdStore);

                //
                // Parse the AP name
                //
                strncpy(pcSsid, acCmdStore, lRetVal);
                if(pcSsid != NULL)
                {
                    ucRecvdAPDetails = 1;
                    pcSsid[lRetVal] = '\0';

                }
            }
        }while(ucRecvdAPDetails == 0);

        //
        // Reset Security Parameters to OPEN security type
        //
        SecurityParams.Key = (signed char *)"";
        SecurityParams.KeyLen = 0;
        SecurityParams.Type = SL_WLAN_SEC_TYPE_OPEN;

        UART_PRINT("\n\rTrying to connect to AP: %s ...\n\r",pcSsid);

        //
        // Get the current timer tick and setup the timeout accordingly
        //
        usConnTimeout = g_usConnectIndex + 15;

        //
        // This triggers the CC3220 to connect to specific AP
        //
        lRetVal = sl_WlanConnect((signed char *)pcSsid,
                                  strlen((const char *)pcSsid), NULL,
                                  &SecurityParams, NULL);
        ASSERT_ON_ERROR(lRetVal);

        //
        // Wait ~10 sec to check if connection to specifed AP succeeds
        //
        while(!(IS_CONNECTED(g_ulStatus)) || !(IS_IP_ACQUIRED(g_ulStatus)))
        {
#ifndef SL_PLATFORM_MULTI_THREADED
            _SlNonOsMainLoopTask();
#else
              osi_Sleep(1);
#endif
            ROM_UtilsDelayDirect(16000000);
            if(g_usConnectIndex >= usConnTimeout)
            {
                break;
            }
            g_usConnectIndex++;
        }

    }
#endif
    //
    // Put message on UART
    //
    UART_PRINT("\n\rDevice has connected to %s\n\r",pcSsid);

    //
    // Get IP address
    //
    lRetVal = Network_IF_IpConfigGet(&ulIP,&ulSubMask,&ulDefGateway,&ulDns);
    ASSERT_ON_ERROR(lRetVal);

    //
    // Send the information
    //
    UART_PRINT("Device IP Address is %d.%d.%d.%d \n\r\n\r",
            SL_IPV4_BYTE(ulIP, 3),SL_IPV4_BYTE(ulIP, 2),
            SL_IPV4_BYTE(ulIP, 1),SL_IPV4_BYTE(ulIP, 0));
    return 0;
}

//*****************************************************************************
//
//! Disconnect  Disconnects from an Access Point
//!
//! \param  none
//!
//! \return 0 disconnected done, other already disconnected
//
//*****************************************************************************
long
Network_IF_DisconnectFromAP()
{
    long lRetVal = 0;

    if (IS_CONNECTED(g_ulStatus))
    {
        lRetVal = sl_WlanDisconnect();
        if(0 == lRetVal)
        {
            // Wait
            while(IS_CONNECTED(g_ulStatus))
            {
    #ifndef SL_PLATFORM_MULTI_THREADED
                  _SlNonOsMainLoopTask();
    #else
                  osi_Sleep(1);
    #endif
            }
            return lRetVal;
        }
        else
        {
            return lRetVal;
        }
    }
    else
    {
        return lRetVal;
    }

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
//! \return On success, zero is returned. On error, -1 is returned
//
//*****************************************************************************
long
Network_IF_IpConfigGet(unsigned long *pulIP, unsigned long *pulSubnetMask,
                unsigned long *pulDefaultGateway, unsigned long *pulDNSServer)
{
    unsigned short usDHCP = 0;
    long lRetVal = -1;
    unsigned short len = sizeof(SlNetCfgIpV4Args_t);
    SlNetCfgIpV4Args_t ipV4 = {0};

    // get network configuration
    lRetVal = sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE,&usDHCP,&len,
                            (unsigned char *)&ipV4);
    ASSERT_ON_ERROR(lRetVal);

    *pulIP=ipV4.Ip;
    *pulSubnetMask=ipV4.IpMask;
    *pulDefaultGateway=ipV4.IpGateway;
    *pulDefaultGateway=ipV4.IpDnsServer;

    return lRetVal;
}

//*****************************************************************************
//
//! Network_IF_GetHostIP
//!
//! \brief  This function obtains the server IP address using a DNS lookup
//!
//! \param[in]  pcHostName        The server hostname
//! \param[out] pDestinationIP    This parameter is filled with host IP address.
//!
//! \return On success, +ve value is returned. On error, -ve value is returned
//!
//
//*****************************************************************************
long Network_IF_GetHostIP( char* pcHostName,unsigned long * pDestinationIP )
{
    long lStatus = 0;

    lStatus = sl_NetAppDnsGetHostByName((signed char *) pcHostName,
                                            strlen(pcHostName),
                                            pDestinationIP, SL_AF_INET);
    ASSERT_ON_ERROR(lStatus);

    UART_PRINT("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
                    pcHostName, SL_IPV4_BYTE(*pDestinationIP,3),
                    SL_IPV4_BYTE(*pDestinationIP,2),
                    SL_IPV4_BYTE(*pDestinationIP,1),
                    SL_IPV4_BYTE(*pDestinationIP,0));
    return lStatus;

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
    g_ulStatus = 0;
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
unsigned long
Network_IF_CurrentMCUState()
{
    return g_ulStatus;
}
//*****************************************************************************
//
//!  \brief  sets a state from the state machine
//!
//!  \param  cStat Status of State Machine defined in e_StatusBits
//!
//!  \return none
//!
//*****************************************************************************
void 
Network_IF_SetMCUMachineState(char cStat)
{
    SET_STATUS_BIT(g_ulStatus, cStat);
}

//*****************************************************************************
//
//!  \brief  Unsets a state from the state machine
//!
//!  \param  cStat Status of State Machine defined in e_StatusBits
//!
//!  \return none
//!
//*****************************************************************************
void 
Network_IF_UnsetMCUMachineState(char cStat)
{
    CLR_STATUS_BIT(g_ulStatus, cStat);
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
unsigned short itoa(short cNum, char *cString)
{
    char* ptr;
    short uTemp = cNum;
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


