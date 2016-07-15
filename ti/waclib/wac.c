//*****************************************************************************
// Copyright (C) 2014 Texas Instruments Incorporated
//
// All rights reserved. Property of Texas Instruments Incorporated.
// Restricted rights to use, duplicate or disclose this code are
// granted through contract.
// The program may not be used without the written permission of
// Texas Instruments Incorporated or against the terms and conditions
// stipulated in the agreement under which this program has been supplied,
// and under no circumstances can it be used with non-TI connectivity device.
//
//*****************************************************************************

#include <stdio.h>
#include <stdlib.h>

#include "mfilib.h"
#include "wac.h"
#include "waclib.h"
#include "wac_user.h"

#include "simplelink.h"
#include "errors.h"

//*****************************************************************************
//MACROS
//*****************************************************************************

//#define ENABLE_IPV6_GLOBAL_ADDR // enable this for enabling global IPv6 address.
// Date : 25/ May/ 2015: this is a temporary fix.
//Aqcuiring IPV6 global address could not be tested yet.

#define esprintf sl_ExtLib_UART_Print

#define MIN(a,b) (b^((a^b)&-(a<b)))

#define WLAN_PACKET_SIZE        1408
#define AP_SSID_LEN_MAX         (33)
#define SL_DEVICE_GET_STATUS    (2)

#define ERR_PRINT(x) DBG_PRINT("Error [%d] at line [%d] in function [%s]  \n\r",x,__LINE__,__FUNCTION__)

// Loop forever, user can change it as per application's requirement
#define WAC_LOOP_FOREVER(line_number) \
            {\
                while(1); \
            }

// check the error code and handle it
#define WAC_ASSERT_ON_ERROR(error_code) \
            {\
                 if(error_code < 0) \
                   {\
                        ERR_PRINT(error_code);\
                        return error_code;\
                 }\
            }

#define WAC_MEM_ERROR_CHECK(mem_pointer)    \
                if(mem_pointer == NULL)\
                  {\
                        goto _error_exit;\
                  }

#define WAC_MEM_FREE(mem_pointer)   \
                if(mem_pointer != NULL)\
                  {\
                    free(mem_pointer);\
                    mem_pointer = NULL;\
                  }
                  

#ifdef DEBUG_PRINTS 
#define DBG_PRINT       sl_ExtLib_UART_Print
#else 
#define DBG_PRINT(x,...) 
#endif

#ifdef SL_LLA_SUPPORT
#define ARP_HDR_LEN 	(28)
#define ETH_HDR_LEN 	(14)      // Ethernet header length
#define LLA_IP_ADDRESS_START    0xa9FE0100 /* 169.254.1.0*/
#endif

#define WAC_MDNS_STOP_SERVICE()   (mDnsRunStatus?_sl_ExtLib_WAC_Stop_MDNS():0)
#define WAC_MDNS_START_SERVICE(x)  ((!mDnsRunStatus)?_sl_ExtLib_WAC_Start_MDNS(x):x)

//*****************************************************************************
// Global and Static Variables and structures
//*****************************************************************************

// Below extern variables are to be declared in the file where the async events of capturing the IPv4 and IPV6 acquired events.
// These variables should be initailly 0, and are to be set to 1 when corresponding event occurs.
// This is a temporary fix as the APIs to read IPv6 address are not working in R2 SL v24
extern volatile int g_ipv4_acquired;
extern volatile int g_ipv6_acquired;

volatile int g_currSimplelinkRole = -1;
volatile static uint32_t sl_started_flag = 0; // 0 = not started; 1 = started


//Global user data structure (pointers to user data)
static _sl_ExtLib_WAC_User_Data_t g_WACUserData; 
//Global parameter to store for user (such as playPassowrd)
static _sl_ExtLib_WAC_GlobalData_t g_WAC_GlobalData; 
// Global parameter to store MFI data
static _sl_ExtLib_MFi_Data_t *g_pMFiData;
// Global parameter to store WAC data
static _sl_ExtLib_WAC_Data_t *g_pWACData;

// Global parameter to hold the device URN name
static unsigned char g_CurrDevURNname[MAX_DEVICE_URN_LENGTH] = {0};

// If application doesn't provide any private key then a fixed private key (worked with CC3200)
static unsigned char g_ucOurPrivateKey[WAC_PRIVATE_KEY_LEN]={0xAA, 0xBB, 0xCC, 0xDD,
                        0xEE, 0xFF, 0x00, 0x11, 0x22, 
                        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 
                        0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 
                        0x55, 0x66, 0x77, 0x88, 0x99};
  
static unsigned char *g_pWACControlBuf = NULL;
static unsigned char *g_pMacAddress = NULL;
static unsigned char g_ucMacAddressLen = SL_MAC_ADDR_LEN;
static volatile unsigned long g_ulWacNwStatus = 0;
static volatile unsigned char mDnsRunStatus = 0; 

#ifdef SL_LLA_SUPPORT

typedef struct  {
  unsigned short hwtype;
  unsigned short prototype;
  unsigned char hwlen;
  unsigned char protolen;
  unsigned short opcode;
  unsigned char senderMac[6];
  unsigned char senderIp[4];
  unsigned char targetMac[6];
  unsigned char targetIp[4];
}slArpHdr;

#endif



//*****************************************************************************
// Function Prototypes
//*****************************************************************************
static signed long _sl_ExtLib_WAC_initUserDataStructure();  
static signed long _sl_ExtLib_WAC_initWacData();
static signed long _sl_ExtLib_WAC_GetTLV (unsigned char buf_in[], _sl_ExtLib_WAC_Data_t *WACData);
static signed long _sl_ExtLib_WAC_ParseTLV (_sl_ExtLib_WAC_Data_t *WACData);
static signed long _sl_ExtLib_WAC_rtsp_BuildPostResponse(unsigned char *buf_out, 
                                          unsigned char *response, int size,
                                          unsigned char *clientPublicKey, 
                                          signed short clientPublicKeySize, 
                                          _sl_ExtLib_WAC_Data_t *WACData, 
                                          unsigned short certificateDataLength, 
                                          unsigned char* certificateData);
static signed long _sl_ExtLib_WAC_rtsp_BuildConfigResponse(unsigned char *buf_out, 
                                                 unsigned char *response, 
                                                _sl_ExtLib_WAC_Data_t *WACData);
static signed long _sl_ExtLib_WAC_rtsp_BuildConfiguredResponse (unsigned char *buf_out, unsigned char *response);
static signed long _sl_ExtLib_WAC_Build_IE(_sl_ExtLib_WAC_IE_t* wacIE); //Consider changing the name of this function
static signed long _sl_ExtLib_WAC_Init_Socket(signed short* WACListeningSockID, int SL_Role);
static signed long _sl_ExtLib_WAC_Open_Socket(unsigned short Port, signed short* WACListeningSockID, int SL_Role);
static signed long _sl_ExtLib_WAC_Set_AP_Params(_sl_ExtLib_WAC_IE_t* wacIE);
static signed long _sl_ExtLib_WAC_Start_MDNS(signed char configured);
static signed long _sl_ExtLib_WAC_Stop_MDNS(void);
static signed long _sl_ExtLib_SetString(unsigned char* pValue, unsigned short*Length, unsigned short maxLength);


//*****************************************************************************
// Function Definitions
//*****************************************************************************
/*
 *  ======== SL_start Call back========
 */
void SimpleLinkInitCallback(_u32 status)
{
    sl_started_flag = 1;
    g_currSimplelinkRole = status;
}

/* this function will be used instead of sl_start only where we need to ensure 
* g_currSimplelinkRole gets updated before being used in wlan_connected or 
* ip_acquired event handlers.
* This function is as such blocked till sl_start happens
*/
void sl_Start_custom(void)
{ 
    sl_Start(NULL, NULL, (P_INIT_CALLBACK)SimpleLinkInitCallback);

    while(0 == sl_started_flag)
    {
        /*do nothing*/
    }

    // make the below flag zero for next usage
    sl_started_flag = 0;
}



//****************************************************************************
//
//! This function is to Stop existing mDNS service on the device
//!
//! \param[in] None
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Stop_MDNS()
{
    unsigned char  *deviceURN = NULL;
    unsigned char  deviceURNLen = MAX_DEVICE_URN_LENGTH; 
    unsigned char  *serviceName = NULL;
    signed long   lRetVal = 0;

    deviceURN = (unsigned char*) malloc(MAX_DEVICE_URN_LENGTH);
    if(deviceURN == NULL)
    {
        lRetVal = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    
    // Get the service name which is going to be unregistered
    lRetVal = sl_NetAppGet (SL_NETAPP_DEVICE_ID, \
    						SL_NETAPP_DEVICE_URN, \
                            &deviceURNLen, deviceURN);
    if(lRetVal < 0)
    {
        goto _error_exit;
    }
    
    serviceName = (unsigned char*) malloc(deviceURNLen + sizeof(WAC_SERVICE_NAME));
    if(serviceName == NULL)
    {
        lRetVal = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    strcpy ((char*)serviceName, (char const*)deviceURN);
    strcat ((char*)serviceName, WAC_SERVICE_NAME);

    DBG_PRINT("Unregistering WAC mDNS service : %s\n\r", serviceName);
    
    // Unregister the existing mDNS service
    //lRetVal = sl_NetAppMDNSUnRegisterService((signed char const*)serviceName, \
                                                strlen((char const*)serviceName)); // R1 SL
	lRetVal = sl_NetAppMDNSUnRegisterService((signed char const*)serviceName, \
												strlen((char const*)serviceName), 0); // R2 SL
_error_exit:  
    WAC_MEM_FREE(deviceURN);
    WAC_MEM_FREE(serviceName);
    
    //if(lRetVal == 0 || lRetVal == SL_NET_APP_DNS_NOT_EXISTED_SERVICE_ERROR) // R1 SL
    //if(lRetVal == 0 || lRetVal == SL_NET_APP_MDNS_EXISTED_SERVICE_ERROR)// R2 SL v19
    if(lRetVal == 0 || lRetVal == SL_ERROR_NET_APP_MDNS_EXISTED_SERVICE_ERROR)// R2 SL v24
    {
        // Set status to mDNS running
        mDnsRunStatus = 0;
        // return as success
        return EXTLIB_WAC_SUCCESS;
    }
    
    return lRetVal;
}


//****************************************************************************
//
//! This function is to start mDNS service with seed = 0\1
//!
//! \param[in] option whether start mDNS as configured/unconfigured device
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Start_MDNS(signed char configured)
{
    //84 out of 100 bytes in use. No variables, only constants
    unsigned char *serviceText = NULL;
    //Name comprised of URN (with its set max) and fixed service name
    unsigned char  *serviceName = NULL; 
    unsigned int offset=0;
    signed long lRetVal=0;
    unsigned char  deviceURNLen = MAX_DEVICE_URN_LENGTH + sizeof(WAC_SERVICE_NAME);

    serviceName = (unsigned char*) malloc(MAX_DEVICE_URN_LENGTH + sizeof(WAC_SERVICE_NAME));
    if(serviceName == NULL)
    {
        lRetVal = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }

    serviceText = (unsigned char*) malloc(WAC_MDNS_SERVICE_TEXT_LEN);
    if(serviceText == NULL)
    {
        lRetVal = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }

    // device ID field
    strcpy((char*)serviceText + offset, DEVICEID);
    offset += strlen(DEVICEID);
    if(g_pMacAddress != NULL)
    {
        offset += esprintf((char*)serviceText + offset, "%02x:%02x:%02x:%02x:%02x:%02x", \
                            g_pMacAddress[0],g_pMacAddress[1],g_pMacAddress[2],\
                            g_pMacAddress[3],g_pMacAddress[4],g_pMacAddress[5]);
    }
    else
    {
        lRetVal = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    
    // features field
    strcpy((char*)serviceText + offset, FEATURES);
    offset += strlen(FEATURES);

    strcpy((char*)serviceText + offset++, "0x4"); //Documentaion seem to suggest hex value to be inserted into a text record. I'm using text instead
    offset += 2;
    // flags field
    strcpy((char*)serviceText + offset, FLAGS);
    offset += strlen(FLAGS);

    strcpy((char*)serviceText + offset++, "0x2"); //Documentaion seem to suggest hex value to be inserted into a text record. I'm using text instead
    offset += 2;
    
    // protocol version  
    strcpy((char*)serviceText + offset, PROTOVERS);
    offset += strlen(PROTOVERS);
    strcpy((char*)serviceText + offset, PROTOCOLVER);
    offset += strlen(PROTOCOLVER);  

    // seed  
    strcpy((char*)serviceText + offset, SEED);
    offset += strlen(SEED);

    if (!configured)
    {
        strcpy((char*)serviceText + offset++, "0"); //At this initial point, seed will always be zero
    }
    else 
    {
        strcpy((char*)serviceText + offset++, "1"); //Once in STA mode and connected to the target AP, seed should be 1
    }

    // Source version number
    strcpy((char*)serviceText + offset, SRCVERS);
    offset+=strlen(SRCVERS);
    strcpy((char*)serviceText + offset, SOURCEVER);
    offset+=strlen(SOURCEVER);  

    strcpy((char*)serviceText + offset++, ";"); //Final terminator
    serviceText[offset]=0; //Final terminator

	// Get Device URN name
	lRetVal = sl_NetAppGet (SL_NETAPP_DEVICE_ID, \
						SL_NETAPP_DEVICE_URN, \
						&deviceURNLen, serviceName);
	if(lRetVal < 0)
	{
		goto _error_exit;
	}

    // Appending WAC service name to device's URN name
    strcat((char*)serviceName, WAC_SERVICE_NAME);

    DBG_PRINT("Registering MDNS service %s\n\r", serviceName);

    //lRetVal = sl_NetAppMDNSRegisterService((signed char const*)serviceName, strlen((char const*)serviceName),\
                                        (signed char const*)serviceText, strlen((const char*)serviceText),\
                                        g_WACUserData.wacPortNo, WAC_TTL, 0);

    if(configured == 0) // LP is in AP mode: use IPV4 as IPv6 is not supported in AP mode
	{
		lRetVal = sl_NetAppMDNSRegisterService((signed char const*)serviceName, strlen((char const*)serviceName),\
										(signed char const*)serviceText, strlen((const char*)serviceText),\
										g_WACUserData.wacPortNo, WAC_TTL, 0);
	}
	else // configured = 1 ; LP is in STA mode connected to target AP ;
	{
		//lRetVal = sl_NetAppMDNSRegisterService((signed char const*)serviceName, strlen((char const*)serviceName),\
												(signed char const*)serviceText, strlen((const char*)serviceText),\
												g_WACUserData.wacPortNo, WAC_TTL, NETAPP_MDNS_IPV6_ONLY_SERVICE);
		//lRetVal = sl_NetAppMDNSRegisterService((signed char const*)serviceName, strlen((char const*)serviceName),\
														(signed char const*)serviceText, strlen((const char*)serviceText),\
														g_WACUserData.wacPortNo, WAC_TTL, NETAPP_MDNS_IPV4_ONLY_SERVICE);
		lRetVal = sl_NetAppMDNSRegisterService((signed char const*)serviceName, strlen((char const*)serviceName),\
														(signed char const*)serviceText, strlen((const char*)serviceText),\
														g_WACUserData.wacPortNo, WAC_TTL, SL_NETAPP_MDNS_IPV6_IPV4_SERVICE);
		//lRetVal = sl_NetAppMDNSRegisterService((signed char const*)serviceName, strlen((char const*)serviceName),\
																(signed char const*)serviceText, strlen((const char*)serviceText),\
																g_WACUserData.wacPortNo, WAC_TTL, 0);
	}


    //if (lRetVal < 0 && lRetVal != SL_NET_APP_DNS_IDENTICAL_SERVICES_ERROR) // R1 SL
    //if (lRetVal < 0 && lRetVal != SL_NET_APP_MDNS_IDENTICAL_SERVICES_ERROR) // R2 SL v19
    if (lRetVal < 0 && lRetVal != SL_ERROR_NET_APP_MDNS_IDENTICAL_SERVICES_ERROR) // R2 SL v24
    {
        ERR_PRINT(lRetVal);
        lRetVal = EXTLIB_WAC_ERROR_REGISTERING_MDNS;
        goto _error_exit;
    }
    
    lRetVal = 0;
    
    // Starting mDNS service
    sl_NetAppStart(SL_NETAPP_MDNS_ID);
    //TODO need handle errors here
    if(lRetVal < 0)
    {
       // ERR_PRINT(lRetVal);
        //lRetVal = EXTLIB_WAC_ERROR_STARTING_MDNS;
        //goto _error_exit;
    }
    // Set status to mDNS running
    mDnsRunStatus = 1;
    
_error_exit:
    WAC_MEM_FREE(serviceText);
    WAC_MEM_FREE(serviceName);
    
    lRetVal = (lRetVal < 0)?lRetVal:EXTLIB_WAC_SUCCESS; 
    return lRetVal;
}


//****************************************************************************
//
//! This function is to reset WLAN general parameters and information elements
//!
//! \param[in] None
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Reset_AP_Params()
{
	signed long lRetVal;

	//sl_protocol_WlanSetInfoElement_t    infoele; // R2 SL v19
	//memset(&infoele, 0, sizeof(sl_protocol_WlanSetInfoElement_t)); // R2 SL v19
	// lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
						WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT, \
						(sizeof(sl_protocol_WlanSetInfoElement_t)), \
						(unsigned char*) &infoele); // R2 SL v19

	//SlWlanProtocolSetInfoElement_t infoele; // R2 SL v24
	//memset(&infoele, 0, sizeof(SlWlanProtocolSetInfoElement_t)); // R2 SL v24
	//lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
							WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT, \
							(sizeof(SlWlanProtocolSetInfoElement_t)), \
							(unsigned char*) &infoele); // R2 SL v24


	SlWlanSetInfoElement_t infoele; // R2 SL v29
	memset(&infoele, 0, sizeof(SlWlanSetInfoElement_t)); // R2 SL v29
	lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
							SL_WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT, \
								(sizeof(SlWlanSetInfoElement_t)), \
								(unsigned char*) &infoele); // R2 SL v29

	WAC_ASSERT_ON_ERROR(lRetVal);

	return EXTLIB_WAC_SUCCESS;
}


//****************************************************************************
//
//! This function is to set the IE parameters in the access point mode
//!
//! \param[in] WAC IE data structure
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Set_AP_Params(_sl_ExtLib_WAC_IE_t* wacIE)
{
	//unsigned char  ucSecType = SL_SEC_TYPE_OPEN; // R2 V29
	unsigned char  ucSecType = SL_WLAN_SEC_TYPE_OPEN; // R2 v32
	signed long lRetVal;

	//volatile sl_protocol_WlanSetInfoElement_t    infoele, infoele_dbg; // R2 SL v19
	//volatile SlWlanProtocolSetInfoElement_t    infoele, infoele_dbg; // R2 SL v24
	volatile SlWlanSetInfoElement_t    infoele; // R2 SL v29

	infoele.Index     = 0;
	//infoele.Role      = SL_INFO_ELEMENT_AP_ROLE; // R2 SL v29
	infoele.Role      =	SL_WLAN_INFO_ELEMENT_AP_ROLE; // R2 SL v32
	infoele.IE.Id     = 0;
	infoele.IE.Oui[0] = 0;
	infoele.IE.Oui[1] = 160;
	infoele.IE.Oui[2] = 64;
	infoele.IE.Length = wacIE->length;

	//memset((void *)infoele.IE.Data, 0, SL_INFO_ELEMENT_MAX_SIZE); // R2 SL v29
	memset((void *)infoele.IE.Data, 0, SL_WLAN_INFO_ELEMENT_MAX_SIZE); // R2 SL v32
	memcpy((void *)infoele.IE.Data, wacIE->IE, wacIE->length);

	//lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
						WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT, \
						(sizeof(sl_protocol_WlanSetInfoElement_t)), \
						(unsigned char*) &infoele); // R2 SL v19

	//lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
							WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT, \
							(sizeof(SlWlanProtocolSetInfoElement_t)), \
							(unsigned char*) &infoele); // R2 SL v24

	lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, \
								SL_WLAN_GENERAL_PARAM_OPT_INFO_ELEMENT, \
								(sizeof(SlWlanSetInfoElement_t)), \
								(unsigned char*) &infoele); // R2 SL v29

	WAC_ASSERT_ON_ERROR(lRetVal);

	// Device must be set as Open AP, otherwise the iDevice wouldn't try to connect
	lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, SL_WLAN_AP_OPT_SECURITY_TYPE,\
						1, &ucSecType);
	WAC_ASSERT_ON_ERROR(lRetVal);

	return EXTLIB_WAC_SUCCESS;
}

//****************************************************************************
//
//! This function is to open and bind a TCP Server socket
//!
//! \param[in] Port no. over which socket will open
//! \param[out] listening Socket ID 
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Open_Socket(unsigned short Port, 
                                              signed short* WACListeningSockID, int SL_Role)
{
	signed short    AddrSize;
	    signed short    Status;
	    unsigned long nonBlocking = 1;
	    SlSockAddrIn_t  LocalAddr;
	    SlSockAddrIn6_t LocalAddr_ipV6;

	    if(SL_Role == ROLE_AP)// IPv4
	    {

			LocalAddr.sin_family = SL_AF_INET;
			LocalAddr.sin_port = sl_Htons(Port);
			LocalAddr.sin_addr.s_addr = 0;

			*WACListeningSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
			DBG_PRINT("\n\r Socket handle in AP mode = %d \n\r ", *WACListeningSockID );

			if( *WACListeningSockID < 0 )
			{
				// error
				return ERROR_FAILED_OPENING_SOCKET;
			}

			AddrSize = sizeof(SlSockAddrIn_t);
			Status = sl_Bind(*WACListeningSockID, (SlSockAddr_t *)&LocalAddr, AddrSize);
			if( Status < 0 )
			{
			   ERR_PRINT(Status);
			   return ERROR_FAILED_BINDING_SOCKET;
			}
	    }

	    else // support for IPv6 // Role STA
	    {

	    	// adding support to recv both IP4/IP6 pkts
			*WACListeningSockID = sl_Socket(SL_AF_INET6,SL_SOCK_STREAM, 0);
			if( *WACListeningSockID < 0 )
			{
				// error
				return ERROR_FAILED_OPENING_SOCKET;
			}
			DBG_PRINT("\n\r Socket handle in STA mode = %d \n\r ", *WACListeningSockID );

		   LocalAddr_ipV6.sin6_family = SL_AF_INET6;
		   LocalAddr_ipV6.sin6_port = sl_Htons(Port);
		   LocalAddr_ipV6.sin6_flowinfo = 0;
		   memset(&(LocalAddr_ipV6.sin6_addr), 0, sizeof(SlIn6Addr_t));
		   LocalAddr_ipV6.sin6_scope_id = 0;

		   AddrSize = sizeof(SlSockAddrIn6_t);
		   Status = sl_Bind(*WACListeningSockID, (SlSockAddr_t *)&LocalAddr_ipV6, AddrSize);
		   if( Status < 0 )
		   {
			  ERR_PRINT(Status);
			  return ERROR_FAILED_BINDING_SOCKET;
		   }
	    }


	    Status = sl_Listen(*WACListeningSockID, 0);
	    if( Status < 0 )
	    {
	       ERR_PRINT(Status);
	       return ERROR_FAILED_LISTENING_SOCKET;
	    }

	    Status = sl_SetSockOpt(*WACListeningSockID, SL_SOL_SOCKET, \
	                            SL_SO_NONBLOCKING, \
	                            &nonBlocking, sizeof(nonBlocking));
	    WAC_ASSERT_ON_ERROR(Status);

	    return EXTLIB_WAC_SUCCESS;
}

//****************************************************************************
//
//! This function is to set initialize a listening socket
//!
//! \param[out] socket ID
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Init_Socket(signed short* WACListeningSockID, int SL_Role)
{
  //Open port for WAC control
  return _sl_ExtLib_WAC_Open_Socket(g_WACUserData.wacPortNo, WACListeningSockID, SL_Role);
  
}

//****************************************************************************
//
//! This function is to close existing socket
//!
//! \param[in] socket ID
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Close_Socket(signed short WACListeningSockID)
{
    return sl_Close(WACListeningSockID);  
}


//****************************************************************************
//
//! This function is to build IE data structure
//!
//! \param[out] WAC IE data structure
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_Build_IE (_sl_ExtLib_WAC_IE_t* pWacIE)
{
    signed long idx = 0, lRetVal = -1 ;
    unsigned char deviceURNLen = MAX_DEVICE_URN_LENGTH;

    //Clear IE payload
    memset (pWacIE->IE, 0, sizeof(pWacIE->IE));

    if(g_WACUserData.wacDeviceURNSize)
    {
    	strcpy((char*)pWacIE->name, (const char*)g_WACUserData.wacDeviceURN);
    }
    else
    {
    	//Populate strings in structure
    	lRetVal = sl_NetAppGet(SL_NETAPP_DEVICE_ID, \
    						SL_NETAPP_DEVICE_URN, \
                          &deviceURNLen, (unsigned char*) pWacIE->name);
    	WAC_ASSERT_ON_ERROR(lRetVal);
    }

    //Build information element parameters
    pWacIE->IE[idx++] = 0x00;
    pWacIE->IE[idx++] = IE_Flags; //flags
    pWacIE->IE[idx++] = 0x03; //2 bytes

    pWacIE->IE[idx++] = WAC_IE_FLAG_DEVICE_UNCONFIGURED | WAC_IE_FLAG_SUPPORT_MFI_V1 | \
                      WAC_IE_FLAG_SUPPORT_WOW | WAC_IE_FLAG_WPS_SUPPORT | \
                      (g_WACUserData.flags & 0x000000FF); //Higher byte flags
    pWacIE->IE[idx++] = WAC_IE_FLAG_2_4G_SUPPORT | ((g_WACUserData.flags & 0x0000FF00) >> 8); //Middle byte flags
    pWacIE->IE[idx++] = ((g_WACUserData.flags & 0x00FF0000) >> 16); //Lowest byte flags

    pWacIE->IE[idx++] = IE_Name; //Name
    pWacIE->IE[idx++] = strlen((char const*)pWacIE->name);
    strcpy ((char *)&pWacIE->IE[idx], (char const*)pWacIE->name);
    idx += strlen((char const*)pWacIE->name);

    pWacIE->IE[idx++] = IE_Manufacturer; //Manufacturer
    pWacIE->IE[idx++] = g_WACUserData.wacVendorSize;
    memcpy(&pWacIE->IE[idx], g_WACUserData.wacVendor,g_WACUserData.wacVendorSize);
    idx+=g_WACUserData.wacVendorSize;

    pWacIE->IE[idx++] = IE_Model; //Model
    pWacIE->IE[idx++] = g_WACUserData.wacModelSize;
    memcpy(&pWacIE->IE[idx], g_WACUserData.wacModel,g_WACUserData.wacModelSize);
    idx+=g_WACUserData.wacModelSize;  
    pWacIE->IE[idx++] = IE_OUI; //OUI
    pWacIE->IE[idx++] = 0x03; //OUI length
    pWacIE->IE[idx++] = 0x00; //OUI fixed value
    pWacIE->IE[idx++] = 0xA0; //OUI fixed value
    pWacIE->IE[idx++] = 0x40; //OUI fixed value
    pWacIE->IE[idx++] = IE_MACAddress; //MAC Address
    pWacIE->IE[idx++] = 0x06; //MAC Address length

    memcpy(&pWacIE->IE[idx], g_pMacAddress, g_ucMacAddressLen);
    
    idx += g_ucMacAddressLen;

    pWacIE->length = idx;

    return EXTLIB_WAC_SUCCESS; 
}

//****************************************************************************
//
//! This function is to build a finalized response message 
//!
//! \param[out] output buffer (contains response message)
//! \param[in] input data to build response message
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_rtsp_BuildConfiguredResponse (unsigned char *buf_out, \
                                                            unsigned char *response)
{
    signed long offset = 0;

    memcpy(buf_out, response, strlen((const char*)response)); //First copy the fixed response
    offset += strlen((const char*)response);

    return offset;
}

//****************************************************************************
//
//! This function is to build reponse message during SSID credential exchange 
//!
//! \param[out] output buffer (contains reponse message)
//! \param[in] input buffer which is used to generate output reponse msg
//! \param[in] WAC data structure
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_rtsp_BuildConfigResponse(unsigned char *buf_out, 
                                                 unsigned char *response, 
                                                _sl_ExtLib_WAC_Data_t *WACData)
{
    unsigned char offset = 0;
    unsigned char TLVLenToEncrypt = 0;
    unsigned char TLVResponse[MAX_DEVICE_URN_LENGTH + MAX_INFOAPP_LENGTH + \
                        sizeof(WACData->credentialsStructure.firmwareRevision) + \
                        sizeof(WACData->credentialsStructure.hardwareRevision) + \
                          (5*MAX_STRING_LENGTH) + (9*2)]; 
  
    // Build TLV for response 
    memset (&TLVResponse[0], 0, sizeof(TLVResponse)); 

    if (WACData->credentialsStructure.name[0])
    {
        TLVResponse[offset++] = TLV_name;
        TLVResponse[offset++] = WACData->credentialsStructure.nameLen;
        memcpy(&TLVResponse[offset], &WACData->credentialsStructure.name[0], \
                                    WACData->credentialsStructure.nameLen); 
        offset += WACData->credentialsStructure.nameLen;
    }
  
    if (g_WACUserData.wacInfoAppSize) 
    {
        TLVResponse[offset++] = TLV_bundleSeedID;
        TLVResponse[offset++] = g_WACUserData.wacInfoAppSize;
        memcpy(&TLVResponse[offset], g_WACUserData.wacInfoApp, \
                                    g_WACUserData.wacInfoAppSize); 
        offset+=g_WACUserData.wacInfoAppSize;
    }
  
    if (WACData->credentialsStructure.firmwareRevision[0])
    {
        TLVResponse[offset++] = TLV_firmwareRevision;
        TLVResponse[offset++] = strlen((char const*)WACData->credentialsStructure.firmwareRevision);
        memcpy(&TLVResponse[offset], &WACData->credentialsStructure.firmwareRevision[0], \
                                strlen((char const*)WACData->credentialsStructure.firmwareRevision)); 
        offset+=strlen((char const*)WACData->credentialsStructure.firmwareRevision);
    }
  
    if (WACData->credentialsStructure.hardwareRevision[0])
    {
        TLVResponse[offset++] = TLV_hardwareRevision;
        TLVResponse[offset++] = strlen((char const*)WACData->credentialsStructure.hardwareRevision);
        memcpy(&TLVResponse[offset], &WACData->credentialsStructure.hardwareRevision[0], \
                                    strlen((char const*)WACData->credentialsStructure.hardwareRevision)); 
        offset+=strlen((char const*)WACData->credentialsStructure.hardwareRevision);
    }
    
    if (g_WACUserData.wacVendorSize)
    {
        TLVResponse[offset++] = TLV_manufacturer;
        TLVResponse[offset++] = g_WACUserData.wacVendorSize;
        memcpy(&TLVResponse[offset], g_WACUserData.wacVendor, \
                                    g_WACUserData.wacVendorSize); 
        offset+=g_WACUserData.wacVendorSize;
    }
  
    if (g_WACUserData.wacInfoSerialNumberSize)
    {
        TLVResponse[offset++] = TLV_serialNumber;
        TLVResponse[offset++] = g_WACUserData.wacInfoSerialNumberSize;
        memcpy(&TLVResponse[offset], g_WACUserData.wacInfoSerialNumber, \
                                        g_WACUserData.wacInfoSerialNumberSize); 
        offset+=g_WACUserData.wacInfoSerialNumberSize;
    }
  
    if (g_WACUserData.wacLangSize)
    {
        TLVResponse[offset++] = TLV_language;
        TLVResponse[offset++] = g_WACUserData.wacLangSize;
        memcpy(&TLVResponse[offset], g_WACUserData.wacLang, g_WACUserData.wacLangSize); 
        offset+=g_WACUserData.wacLangSize;
    }
  
    if (g_WACUserData.wacMFiProtSize)
    {
        TLVResponse[offset++] = TLV_mfiProtocol;
        TLVResponse[offset++] = g_WACUserData.wacMFiProtSize;
        memcpy(&TLVResponse[offset], g_WACUserData.wacMFiProt, \
                                    g_WACUserData.wacMFiProtSize); 
        offset+=g_WACUserData.wacMFiProtSize;
    }    
  
    if (g_WACUserData.wacModelSize)
    {
        TLVResponse[offset++] = TLV_model;
        TLVResponse[offset++] = g_WACUserData.wacModelSize;
        memcpy(&TLVResponse[offset], g_WACUserData.wacModel, g_WACUserData.wacModelSize); 
        offset+=g_WACUserData.wacModelSize;
    }      
  
    //Store final offset of TLV + pad to the next 16 bytes
    TLVLenToEncrypt = ((offset/16) + 1)*16; 
  
    //Init offset for HTTP response build
    offset = 0; 

    sl_ExtLib_aes_key_t _sl_aes_key;
    unsigned char ecount_buf[SL_EXTLIB_AES_BLOCK_SIZE];

    sl_ExtLib_AES_set_encrypt_key((unsigned char*) WACData->AESKey, 128, &_sl_aes_key); 
  
    memset(ecount_buf, 0, SL_EXTLIB_AES_BLOCK_SIZE);
    // Encrypt TLV reponse
    sl_ExtLib_CTR128_Encrypt(&TLVResponse[0], &TLVResponse[0], TLVLenToEncrypt, \
                    &_sl_aes_key, (unsigned char*) WACData->AESIV, ecount_buf, 0);


    memcpy(buf_out, response, strlen((char const*)response)); //First copy the fixed response
    offset += strlen((char const*)response);
    offset += esprintf((char*)(buf_out + offset), "%d\r\n\r\n", TLVLenToEncrypt); //Add size and two linefeeds to end HTTP header

    memcpy(buf_out + offset, TLVResponse, TLVLenToEncrypt); //Add encrypted TLV structure as HTTP body
    offset+=TLVLenToEncrypt;
  
    return offset; 
}

//****************************************************************************
//
//! This function is to build reponse message before SSID credential exchange 
//!
//! \param[out] output buffer (contains reponse message)
//! \param[in] input buffer which is used to generate output reponse msg
//! \param[in] size of fixed reponse
//! \param[in[ device's private key
//! \param[in] client public key size
//! \param[in] WAC data structure
//! \param[in] certificate data length
//! \param[in] certificate data 
//!
//! \return success:0, else:error code
//
//****************************************************************************
static const unsigned char		kCurve25519BasePoint[ 32 ] = { 9 };
static signed long _sl_ExtLib_WAC_rtsp_BuildPostResponse(unsigned char *buf_out, 
                                          unsigned char *response, int size,
                                          unsigned char *clientPublicKey, 
                                          signed short clientPublicKeySize, 
                                          _sl_ExtLib_WAC_Data_t *WACData, 
                                          unsigned short certificateDataLength, 
                                          unsigned char* certificateData)
{
    signed short offset = 0;
    unsigned char ourPublicKey[WAC_PUBLIC_KEY_LEN];
    unsigned char sharedSecret[WAC_SHARED_KEY_LEN];
    sl_ExtLib_SHA_ctx_t shaContext;
    unsigned char AESIVBackup[16];
    unsigned char tempMFiChallenge[20];
    unsigned char temp[20];
    unsigned char *signature = NULL;
    unsigned char status = 0;
    unsigned short challengeRespLength;
    unsigned char  *challengeResp = NULL;
    signed long lRetVal = -1;
    sl_ExtLib_aes_key_t  _sl_aes_key;
    unsigned char ecount_buf[SL_EXTLIB_AES_BLOCK_SIZE];

    signature = (unsigned char*) malloc(WAC_SIGNATURE_LEN);
    if(signature == NULL)
    {
        offset = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    memset(signature, 0, WAC_SIGNATURE_LEN);
    
    challengeResp = (unsigned char*) malloc(WAC_SIGNATURE_LEN);
    if(challengeResp == NULL)
    {
        offset = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    memset(&challengeResp[0], 0, WAC_SIGNATURE_LEN);
    
    // Generate keys 
    sl_ExtLib_Curve_Donna(&ourPublicKey[0], &g_ucOurPrivateKey[0], kCurve25519BasePoint);
    sl_ExtLib_Curve_Donna(&sharedSecret[0], &g_ucOurPrivateKey[0], &clientPublicKey[0]);

    // Generating AES-KEY  
    sl_ExtLib_SHA_Init(&shaContext);
    sl_ExtLib_SHA_Update(&shaContext, "AES-KEY", (sizeof("AES-KEY") - 1));
    sl_ExtLib_SHA_Update(&shaContext, &sharedSecret[0], sizeof(sharedSecret));
    sl_ExtLib_SHA_Final(&temp[0], &shaContext);  

    // Copy generated AES-KEY to WACData structure
    memcpy(WACData->AESKey, &temp[0], sizeof(WACData->AESKey));  

    // Generating AES-IV
    sl_ExtLib_SHA_Init(&shaContext);
    sl_ExtLib_SHA_Update(&shaContext, "AES-IV", (sizeof("AES-IV") - 1));
    sl_ExtLib_SHA_Update(&shaContext, &sharedSecret[0], sizeof(sharedSecret));
    sl_ExtLib_SHA_Final(&temp[0], &shaContext);  

    // Copy generated AES-IV to WACData structure
    memcpy(WACData->AESIV, &temp[0], sizeof(WACData->AESIV));  

    // Take a backup of AES-IV
    memcpy(&AESIVBackup[0], &temp[0], sizeof(AESIVBackup));  

    // Generating MFI challenge key using Public Key
    sl_ExtLib_SHA_Init(&shaContext);
    sl_ExtLib_SHA_Update(&shaContext, &ourPublicKey[0], sizeof(ourPublicKey));
    sl_ExtLib_SHA_Update(&shaContext, &clientPublicKey[0], clientPublicKeySize);
    sl_ExtLib_SHA_Final(&temp[0], &shaContext);  

    memcpy(&tempMFiChallenge[0], &temp[0], sizeof(tempMFiChallenge));

    challengeRespLength = sl_ExtLib_MFi_Get (MFI_HASH_DATA, (unsigned char*)&tempMFiChallenge[0], \
                                            sizeof(tempMFiChallenge), \
                                            &challengeResp[0], \
                                            sizeof(challengeResp));

    lRetVal = sl_ExtLib_MFi_Get(MFI_READ_ERROR_REGISTER_VALUE,0 ,0 ,&status, sizeof(status));
    DBG_PRINT("MFi error register status (0 = OK) is %x\n\r", status);
    WAC_ASSERT_ON_ERROR(lRetVal);

    sl_ExtLib_AES_set_encrypt_key((unsigned char*) WACData->AESKey, 128, &_sl_aes_key); 

    memset(ecount_buf, 0, SL_EXTLIB_AES_BLOCK_SIZE);
    
    // Encrypt ChallangeData
    sl_ExtLib_CTR128_Encrypt(&challengeResp[0], &signature[0], 128, &_sl_aes_key, \
                            (unsigned char*) WACData->AESIV, ecount_buf, 0);
  
  
    memcpy(buf_out, response, strlen((char const*)response)); //First copy the fixed response
    offset += strlen((char const*)response);

    lRetVal = esprintf((char*)(buf_out + offset), "%d\r\n", sizeof(ourPublicKey) + 4 + \
                    certificateDataLength + 4 + WAC_SIGNATURE_LEN); //Add size 
    offset += lRetVal; 
  
    //Uncomment if content-type is required after content-length
    /*
    strcpy ((buf_out + offset), "Content-Type:application/octet-stream\r\n");
    offset += strlen("Content-Type:application/octet-stream\r\n");*/

    //Comment from here if CSEQ not required
    /*cseq = strstr(buf_in, "CSeq:"); //Look for CSeq in the original message and copy it to the response
    if (cseq)
    {
        cseq_end = strchr(cseq + 6, '\n'); 
        strncpy((char*)(buf_out + offset), cseq, cseq_end - cseq + 1);

        buf_out[ offset + (int)(cseq_end - cseq) + 1 ] = '\r';
        buf_out[ offset + (int)(cseq_end - cseq) + 2 ] = '\n';
        buf_out[ offset + (int)(cseq_end - cseq) + 3 ] = '\0';     

        offset += ((cseq_end - cseq) + 3);
    }
    else
        return -1;*/
    
    buf_out[offset++] = 0x0D; //Uncomment in case CSEQ not required
    buf_out[offset++] = 0x0A;
  
    /* Build HTTP Body*/

    /* Our public key */
    memcpy(buf_out + offset, &ourPublicKey[0], sizeof(ourPublicKey));
    offset += sizeof(ourPublicKey);

    /* MFi Certificate Length in big-endian format */
    lRetVal = certificateDataLength;
    lRetVal = SL_WAC_Swap32(lRetVal); //Data certificate length should be posted in big-endian format
    memcpy(buf_out + offset, &lRetVal , sizeof(lRetVal));
    offset += sizeof(lRetVal);

    /* MFi Certificate Data */
    memcpy(buf_out + offset, certificateData , certificateDataLength); //Add certificate data
    offset += certificateDataLength;    

    /* Data signature length in big-endian format */
    lRetVal = challengeRespLength;
    lRetVal = SL_WAC_Swap32(lRetVal); //Data signature length should be posted in big-endian format
    memcpy(buf_out + offset, &lRetVal , sizeof(lRetVal));
    offset += sizeof(lRetVal);

    /* Data signature */
    memcpy(buf_out + offset, &signature[0], WAC_SIGNATURE_LEN); //Add certificate data
    offset += WAC_SIGNATURE_LEN;    

_error_exit:
    WAC_MEM_FREE(signature);
    WAC_MEM_FREE(challengeResp);
    
    return offset;
}
   
//****************************************************************************
//
//! This function is to parse TLV data from WAC data recieved from iDevice
//!
//! \param[in] WAC data structure
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_ParseTLV (_sl_ExtLib_WAC_Data_t *WACData)
{
    signed char idx=0;

    WACData->credentialsStructure.keyExists = 0;

    // Currently, the only fields getting from the configuring device are 
    // SSID, PSK, Name and Airplay password 
    while (idx < WACData->TLVLength)
    {
        switch (*(WACData->TLVData + idx))
        {
            case (TLV_name):
                idx++;
                //Need to consider null termination
                memset(WACData->credentialsStructure.name, 0, \
                        WACData->credentialsStructure.nameLen);
                
                WACData->credentialsStructure.nameLen = MIN (MAX_DEVICE_URN_LENGTH-1,(*(WACData->TLVData + idx)));// 1 for null
                
                //Copy data into structure, truncate long names to max name length
                memcpy(WACData->credentialsStructure.name, \
                        (WACData->TLVData + idx + 1), 
                        WACData->credentialsStructure.nameLen); 
                WACData->credentialsStructure.name[WACData->credentialsStructure.nameLen] = '\0';

                //Skip field + length byte
                idx += (*(WACData->TLVData + idx)) + 1;
                break;
            case (TLV_playPassword):
                idx++;
                g_WAC_GlobalData.playPasswordLen = *(WACData->TLVData + idx);
                //Copy data into structure
                memcpy(g_WAC_GlobalData.playPassword, \
                        (WACData->TLVData + idx + 1), \
                        g_WAC_GlobalData.playPasswordLen); 
                g_WAC_GlobalData.playPassword[g_WAC_GlobalData.playPasswordLen] = '\0';
                //Skip field + length byte
                idx += (*(WACData->TLVData + idx)) + 1; 
                break;
            case (TLV_wifiPSK):
                //Target network is password protected
                WACData->credentialsStructure.keyExists=1; 
                idx++;
                WACData->credentialsStructure.wifiPSKLen = *(WACData->TLVData + idx);
                //Copy data into structure
                memcpy(WACData->credentialsStructure.wifiPSK, \
                        (WACData->TLVData + idx + 1), \
                         WACData->credentialsStructure.wifiPSKLen); 
                //Skip field + length byte
                idx += (*(WACData->TLVData + idx)) + 1; 
                break;
            case (TLV_wifiSSID):
                idx++;
                WACData->credentialsStructure.wifiSSIDLen = *(WACData->TLVData + idx);
                //Copy data into structure
                memcpy(WACData->credentialsStructure.wifiSSID, \
                        (WACData->TLVData + idx + 1), \
                         WACData->credentialsStructure.wifiSSIDLen); 
                //Skip field + length byte
                idx += (*(WACData->TLVData + idx)) + 1;
                break;
            default:
                return ERROR_UNKNOWN_ID_IN_TLV;
        }
    }
    return EXTLIB_WAC_SUCCESS;  
}

//****************************************************************************
//
//! This function is to parse TLV data from second response msg recieved 
//!             from iDevice and update WAC data structure 
//!
//! \param[in] response msg received fromm iDevice
//! \param[out] WAC data structure 
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_GetTLV (unsigned char buf_in[], _sl_ExtLib_WAC_Data_t *WACData)
{
    unsigned char *TLVEncryptedDataPtr=0;
    unsigned char *TLVLengthPtr=0;
    unsigned char TLVLengthStr[8]; //String of the content length
    unsigned char idx=0;
    unsigned char *TLVEncryptedData = NULL;

    TLVEncryptedData = (unsigned char*) malloc(TLV_LENGTH);
    if(TLVEncryptedData == NULL)
    {
        return ETXLIB_WAC_ERROR_MEM_ALLOC;
    }
    memset(&TLVEncryptedData[0], 0,TLV_LENGTH);

    // Seach for HTTP body length 
    TLVLengthPtr = (unsigned char*)strstr((char const*)&buf_in[0], CONTENT_LENGTH);  

    if (TLVLengthPtr)
    {
        TLVLengthPtr+=(sizeof(CONTENT_LENGTH) - 1); //Skip the content-length string
        if (*TLVLengthPtr == ' ') TLVLengthPtr++; //In case of space after the ':', skip it
        while (*TLVLengthPtr != 0x0D) //Grab length string
        {
            TLVLengthStr[idx] = *TLVLengthPtr;
            TLVLengthPtr++;
            idx++;
        }
        TLVLengthStr[idx] = 0; //Terminating character
      
        WACData->TLVLength = atoi((const char*)TLVLengthStr);
    }    
    else 
    {
        WAC_MEM_FREE(TLVEncryptedData);
        return ERROR_NO_CONTENT_LENGTH; //Cannot find content-length string, return error
    }
    // Search for the beginning of the HTTP Body - after two linefeeds. 
    //     That's where the TLV data is
    TLVEncryptedDataPtr = (unsigned char*)strstr((char const*)&buf_in[0], "\r\n\r\n");  
  
    if (TLVEncryptedDataPtr)
    {
        TLVEncryptedDataPtr+=4; //Skip both linefeeds
        memcpy(&TLVEncryptedData[0], TLVEncryptedDataPtr, WACData->TLVLength); //Grab TLV Data
    }    
    else 
    {
        WAC_MEM_FREE(TLVEncryptedData);
        return ERROR_NO_TLV_DATA; //Failed to find HTTP body
    }
  
  
    sl_ExtLib_aes_key_t  _sl_aes_key;
    unsigned char ecount_buf[SL_EXTLIB_AES_BLOCK_SIZE];
  
    sl_ExtLib_AES_set_decrypt_key((unsigned char*) WACData->AESKey, 128, &_sl_aes_key); 
  
    memset(ecount_buf, 0, SL_EXTLIB_AES_BLOCK_SIZE);
    // Decrypt TLV data  
    sl_ExtLib_CTR128_Decrypt(&TLVEncryptedData[0], WACData->TLVData, WACData->TLVLength, \
                            &_sl_aes_key, (unsigned char*) WACData->AESIV, ecount_buf, 0);
  
    WAC_MEM_FREE(TLVEncryptedData);
    
    return WACData->TLVLength;
}

//****************************************************************************
//
//! This function is to extract client public key from first post message
//!
//! \param[in] response msg received fromm iDevice
//! \param[out] client public key
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long WACParsePost (unsigned char buf_in[], unsigned char clientPublicKey[])
{
    unsigned char *publickey;

    // Search for the beginning of the HTTP Body - after two linefeeds. 
    //     That's where the key is
    publickey = (unsigned char*)strstr((char const*)&buf_in[0], "\r\n\r\n");

    if (publickey)
    {
        //Skip both linefeeds, plus one byte for version (0x1)
        publickey += 5; 
        //Grab public key
        memcpy(&clientPublicKey[0], publickey, WAC_PUBLIC_KEY_LEN); 
    }  
    else return -1; //error
  
    return WAC_PUBLIC_KEY_LEN;
}


//****************************************************************************
//
//! This function is to get the WAC data as asked from user
//!
//! \param[in] option to choose which data user needs
//! \param[out] output data value
//! \param[out] output data length
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long sl_ExtLib_WacGet(unsigned char Option, unsigned char* pValue,
                                unsigned short* Length)
{
    switch (Option)
    {
        case (SL_WAC_FLAGS):
        {
            *Length = sizeof(unsigned int); //Three bytes for flags, padding 1 byte
            *((unsigned int*)pValue) = g_WACUserData.flags;
            break;
        }
        case (SL_WAC_VENDOR):
        {
            *Length = g_WACUserData.wacVendorSize;
            memcpy ((char *)pValue, (char const*)g_WACUserData.wacVendor, g_WACUserData.wacVendorSize);
            break;
        }    
        case (SL_WAC_MODEL):
        {
            *Length = g_WACUserData.wacModelSize;
            memcpy ((char *)pValue, (char const*)g_WACUserData.wacModel,g_WACUserData.wacModelSize);
            break;
        }        
        case (SL_WAC_INFO_APP):
        {
            *Length = g_WACUserData.wacInfoAppSize;
            memcpy ((char *)pValue, (char const*)g_WACUserData.wacInfoApp, g_WACUserData.wacInfoAppSize);
            break;
        }            
        case (SL_WAC_INFO_MFIPROT):
        {
            *Length = g_WACUserData.wacMFiProtSize;
            memcpy ((char *)pValue, (char const*)g_WACUserData.wacMFiProt, g_WACUserData.wacMFiProtSize);
            break;
        }                
        case (SL_WAC_INFO_LANG):
        {
            *Length = g_WACUserData.wacLangSize;
            memcpy ((char *)pValue, (char const*)g_WACUserData.wacLang, g_WACUserData.wacLangSize);
            break;
        }                    
        case (SL_WAC_INFO_PLAYPASSWORD):
        {
            *Length = g_WAC_GlobalData.playPasswordLen;
            memcpy ((char *)pValue, (char const*)&g_WAC_GlobalData.playPassword[0], g_WAC_GlobalData.playPasswordLen);
            break;
        }
        case (SL_WAC_DEVICE_URN):
		{
        	*Length = g_pWACData->credentialsStructure.nameLen;
        	memcpy((char*)pValue, (char const*)g_pWACData->credentialsStructure.name,\
                                        g_pWACData->credentialsStructure.nameLen);
        	break;
		}
        default:
            return EXTLIB_WAC_ERROR_UNKNOWN_OPERATION;
            
    }  
  
    return EXTLIB_WAC_SUCCESS;
}

//****************************************************************************
//
//! This function is to set the WAC parameters as per user sends
//!
//! \param[in] option to choose which data user wants to set/update
//! \param[in] input data value
//! \param[in] input data length
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long sl_ExtLib_WacSet(unsigned char Option, void* pValue,
                                unsigned short Length)
{  
    signed long lStatus; 

    switch(Option)
    {
        case (SL_WAC_FLAGS):
        {
	        unsigned int uiFlags = (*(unsigned int*) pValue); //Get flags
	        if (uiFlags & (SL_WAC_AIRPLAY | SL_WAC_AIRPRINT | SL_WAC_HOMEKIT))
	        {
	            g_WACUserData.flags |= uiFlags;
	            lStatus = EXTLIB_WAC_SUCCESS;
	        }  
	        else
	            lStatus = EXTLIB_WAC_ERROR_INVALID_FLAGS;
            
            break;
        }
        case (SL_WAC_VENDOR):
        {
            lStatus = _sl_ExtLib_SetString((unsigned char*)pValue, &Length, MAX_STRING_LENGTH);
            if (EXTLIB_WAC_SUCCESS == lStatus)
            {
                g_WACUserData.wacVendor = (unsigned char*)pValue;
                g_WACUserData.wacVendorSize = Length;
            }      
            break;
        }    
        case (SL_WAC_MODEL):
        {
            lStatus = _sl_ExtLib_SetString((unsigned char*)pValue, &Length, MAX_STRING_LENGTH);
            if (EXTLIB_WAC_SUCCESS == lStatus)
            {
                g_WACUserData.wacModel = (unsigned char*)pValue;
                g_WACUserData.wacModelSize = Length;
            }      
            break;
        }        
        case (SL_WAC_INFO_APP):
        {
            lStatus = _sl_ExtLib_SetString((unsigned char*)pValue, &Length, MAX_INFOAPP_LENGTH);
            if (EXTLIB_WAC_SUCCESS == lStatus)
            {
                g_WACUserData.wacInfoApp = (unsigned char*)pValue;
                g_WACUserData.wacInfoAppSize = Length;
            }      
            break;
        }            
        case (SL_WAC_INFO_MFIPROT):
        {
            lStatus = _sl_ExtLib_SetString((unsigned char*)pValue, &Length, MAX_STRING_LENGTH);
            if (EXTLIB_WAC_SUCCESS == lStatus)
            {
                g_WACUserData.wacMFiProt = pValue;
                g_WACUserData.wacMFiProtSize = Length;
            }      
            break;    
        }                
        case (SL_WAC_INFO_LANG):
        {
            lStatus = _sl_ExtLib_SetString((unsigned char*)pValue, &Length, MAX_STRING_LENGTH);
            if (EXTLIB_WAC_SUCCESS == lStatus)
            {
                g_WACUserData.wacLang = (unsigned char*)pValue;
                g_WACUserData.wacLangSize = Length;
            }      
            break;   
        }                    
        case (SL_WAC_INFO_SERIALNUMBER):
        {
            lStatus = _sl_ExtLib_SetString((unsigned char*)pValue, &Length, MAX_STRING_LENGTH);
            if (EXTLIB_WAC_SUCCESS == lStatus)
            {
                g_WACUserData.wacInfoSerialNumber = (unsigned char*)pValue;
                g_WACUserData.wacInfoSerialNumberSize = Length;
            }      
            break;         
        }                        
        case (SL_WAC_INFO_PLAYPASSWORD):
        {
            lStatus = EXTLIB_WAC_ERROR_READONLY_OPERATION;
            break;
        }  
		case (SL_WAC_PORT_NO):
		{
			if(pValue)
			{
				g_WACUserData.wacPortNo = *(unsigned long*)pValue;
			}
			break;
		}
#if 0
		case SL_WAC_DEVICE_URN:
		{
			if(pValue)
			{
				g_WACUserData.wacDeviceURN = malloc(Length);
				memcpy(g_WACUserData.wacDeviceURN, pValue, Length);
				g_WACUserData.wacDeviceURNSize = Length;
			}
			lStatus = sl_NetAppSet (SL_NETAPP_DEVICE_ID, \
									SL_NETAPP_DEVICE_URN, \
									Length, (unsigned char *) pValue);

			break;
		}
#endif
        default:
            lStatus = EXTLIB_WAC_ERROR_UNKNOWN_OPERATION;
            break;       
    }  
    return lStatus;
}

//****************************************************************************
//
//! This function is to set given string to WAC datat structure
//!
//! \param[in] input string
//! \param[in] input string length
//! \param[in] max length
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_SetString(unsigned char* pValue, 
                                        unsigned short *Length, 
                                        unsigned short maxLength)
{
    if (*Length == NULL) 
    {
        return EXTLIB_WAC_ERROR_INVALID_SIZE; //Invalid input size
    }
    else if (*Length > maxLength)
    {
        return EXTLIB_WAC_ERROR_INVALID_SIZE; //Invalid input size
    }
    else 
    {
        // Strip null termination from size value, if it exists
        if ((*(pValue + *Length - 1)) == 0)
        {
            *Length = (*Length - 1);
        }
        else 
        {
            g_WACUserData.wacInfoSerialNumberSize = (*Length);
        }
        DBG_PRINT("User set string parameter = %s\n\r", pValue);
        return EXTLIB_WAC_SUCCESS;
    }  
  
}

#ifdef SL_LLA_SUPPORT   

//****************************************************************************
//
//! This function is to create ARP request packet
//!
//! \param[in] Source MAC address
//! \param[in] Source MAC address length
//! \param[in] Source IP address
//! \param[in] Target IP address
//! \param[out] output ARP packet
//! \param[out] ARP packet length
//!
//! \return none
//
//****************************************************************************
static void _sl_ExtLib_WAC_CreateARPReqPacket(unsigned char* pucInSrcMacAddr, 
                                        unsigned char* pucInTargetMacAddr, 
                                        unsigned long ulSrcIPaddr, 
                                        unsigned long ulTargetIPaddr, 
                                        unsigned char* pcOutARP, 
                                        unsigned int* puiLen)
{
    unsigned char* pucARPData = pcOutARP;
    slArpHdr arpHeader;
    unsigned char ucDstMac[6];

    // Set destination MAC address: broadcast address
    memset(ucDstMac, 0xff, 6 * sizeof(unsigned char));

    //Target MAC Address
    memcpy(pucARPData, ucDstMac, 6 * sizeof(unsigned char));

    //Source MAC Address
    memcpy(pucARPData + 6 * sizeof(unsigned char), pucInSrcMacAddr, \
            6 * sizeof(unsigned char));

    //Payload Type
    pucARPData[12] = 0x08;

    //Payload SubType - ARP
    pucARPData[13] = 0x06;

    // Hardware type
    arpHeader.hwtype = htons (1);

    // Protocol type
    arpHeader.prototype = htons (0x0800);

    // Hardware address length - MAC Address Len
    arpHeader.hwlen = 6;

    // Protocol address length - IPV4
    arpHeader.protolen = 4;

    // OpCode 1 - Request
    arpHeader.opcode = htons (1);

    // Sender MAC address
    memcpy(&arpHeader.senderMac, pucInSrcMacAddr, 6 * sizeof(unsigned char));

    // Target MAC address
    memcpy(&arpHeader.targetMac, pucInTargetMacAddr, 6 * sizeof(unsigned char));

    // Target IP address
    unsigned long ulIP = htonl(ulTargetIPaddr);
    memcpy(arpHeader.targetIp, &ulIP, 4 * sizeof(unsigned char));

    // Source IP address
    ulIP = htonl(ulSrcIPaddr);
    memcpy(arpHeader.senderIp, &ulIP, 4 * sizeof(unsigned char));

    // ARP header
    memcpy(pucARPData + ETH_HDR_LEN, &arpHeader, ARP_HDR_LEN * sizeof(unsigned char));

    *puiLen = ETH_HDR_LEN + ARP_HDR_LEN;

}

//****************************************************************************
//
//! This function is to initialize Raw packet in bypass mode for LLA
//!
//! \param[in] None
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_InitRawPacket()
{
    // Create Raw Socket - Bypass Mode
    g_sRawPacketSD  = sl_Socket( SL_AF_PACKET, SL_SOCK_RAW, 0 );
    if( g_sRawPacketSD  < 0 )
    {
        DBG_PRINT("failed open RAW Packet socket \r\n");
        return -1;
    }
    
    //Make it NonBlocking
    SlSockNonblocking_t enableOption;
    enableOption.NonblockingEnabled = 1;
    WAC_ASSERT_ON_ERROR(sl_SetSockOpt(g_sRawPacketSD ,SL_SOL_SOCKET,SL_SO_NONBLOCKING,\
                        (unsigned char*)&enableOption,sizeof(enableOption)));
    
    return EXTLIB_WAC_SUCCESS;
}

//****************************************************************************
//
//! This function is to close RAW socket
//!
//! \param[in] None
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_CloseRawSocket()
{
    if(g_sRawPacketSD >0)
    {
        sl_Close(g_sRawPacketSD );
        g_sRawPacketSD  = -1;
    }
    return EXTLIB_WAC_SUCCESS;
}

//****************************************************************************
//
//! This function is to generate random no. using MAC address
//!
//! \param[in] MAC address
//!
//! \return random no.
//
//****************************************************************************
static unsigned long _sl_ExtLib_WAC_GenerateRandomNumFromMAC(unsigned char* pucMACAddr)
{
    unsigned long ulMacAddrSeed = (((pucMACAddr[2]&0xFF)<<24)) | \
                                    (((pucMACAddr[3]&0xFF)<<16)) | \
                                    (((pucMACAddr[4]&0xFF)<<8)) | \
                                    (((pucMACAddr[5]&0xFF)));
    
    return (ulMacAddrSeed%65535);

}

//****************************************************************************
//
//! This function is to get the unique IP in the network 
//!     - by sending ARP req and waiting if anyone of same IP response back
//!     - if no one response back within Timeout then make current IP final
//!
//! \param[in] none
//!
//! \return success:0, else error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_ConnectUsingLLA()
{
    unsigned long ulLLAIpAddress = LLA_IP_ADDRESS_START;
    signed long lRetVal = -1;

    unsigned char ucARPTxBuff[100];
    unsigned int puiLenArpBuff = 0;
    unsigned char ucTargetMacAddr[6];
    unsigned char ucRxBuff[100];
    
    unsigned long statusWlan = 0;
    unsigned char ConfigOpt;
    ConfigOpt = SL_EVENT_CLASS_WLAN;
    unsigned char ConfigLen = 64;
    

    SlNetCfgIpV4Args_t ipV4 = {0};
    unsigned short timeoutCtr=0;
    unsigned char len = sizeof(SlNetCfgIpV4Args_t);
    unsigned char dhcpIsOn = 0;
    int iARPTimeout = 0;
    
    unsigned long ulRandumNum;
    
    ulRandumNum = _sl_ExtLib_WAC_GenerateRandomNumFromMAC(g_pMacAddress);

    // Set destination MAC address: Unknown(0)
    memset (ucTargetMacAddr, 0x0, 6 * sizeof(unsigned char));

    //Create RAW Socket
    lRetVal = _sl_ExtLib_WAC_InitRawPacket();
    WAC_ASSERT_ON_ERROR(lRetVal);
    
    // Wait till IP acquired
    while (ipV4.ipV4 == 0)
    {
    	ulLLAIpAddress |= (ulRandumNum&0xFF);
        ulLLAIpAddress |= (ulRandumNum&0xFF00);
        iARPTimeout = 0;
        
        // Create ARP Request for IP - ulLLAIpAddress 
        _sl_ExtLib_WAC_CreateARPReqPacket(g_pMacAddress, ucTargetMacAddr, 0, \
                                        ulLLAIpAddress,ucARPTxBuff,&puiLenArpBuff);
        
        // Send ARP Request
        lRetVal = sl_Send(g_sRawPacketSD ,ucARPTxBuff,puiLenArpBuff,0);
        WAC_ASSERT_ON_ERROR(lRetVal);
        
        DBG_PRINT("ARP REQUEST Sent with IP address: 0x%lx \n\r",ulLLAIpAddress);

        while(iARPTimeout < LLA_ARP_REQ_TIMEOUT)
        {
            lRetVal = sl_Recv(g_sRawPacketSD , ucRxBuff, 100, 0);
            if(lRetVal>0)
            {
                if(ucRxBuff[13] == 0x06 && ucRxBuff[21] == 0x02)
                {
                    DBG_PRINT("ARP RESPONSE Received\n\r");
                    break;
                }
            }
            
            //Resend ARP Request
            if(iARPTimeout == LLA_ARP_REQ_TIMEOUT/2 || iARPTimeout == LLA_ARP_REQ_TIMEOUT/4 )
            {
                lRetVal = sl_Send(g_sRawPacketSD ,ucARPTxBuff,puiLenArpBuff,0);
                WAC_ASSERT_ON_ERROR(lRetVal);
            }
            
            //Delay ~1msec
            sl_ExtLib_Time_Delay(1);
            iARPTimeout++;
        }
        if(iARPTimeout >= LLA_ARP_REQ_TIMEOUT)
        {
            DBG_PRINT("No ARP RESPONSE Received\n\r");

             //Send Gratuitous ARP
            _sl_ExtLib_WAC_CreateARPReqPacket(g_pMacAddress,ucTargetMacAddr,\
                                                ulLLAIpAddress,ulLLAIpAddress,\
                                                ucARPTxBuff,&puiLenArpBuff);
            lRetVal = sl_Send(g_sRawPacketSD ,ucARPTxBuff,puiLenArpBuff,0);
            WAC_ASSERT_ON_ERROR(lRetVal);
            
            _sl_ExtLib_WAC_CloseRawSocket();

            ipV4.ipV4          = ulLLAIpAddress;
            ipV4.ipV4Mask      = (_u32)SL_IPV4_VAL(255,255,0,0);         // _u32 Subnet mask for this STA/P2P
            ipV4.ipV4Gateway   = 0;              // _u32 Default gateway address
            ipV4.ipV4DnsServer = 0;          // _u32 DNS server address
            lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_STATIC_ENABLE,\
                                    IPCONFIG_MODE_ENABLE_IPV4,\
                                    sizeof(SlNetCfgIpV4Args_t),\
                                    (unsigned char*)&ipV4);
            WAC_ASSERT_ON_ERROR(lRetVal);

            lRetVal = sl_Stop(300);
            
            statusWlan = 0;
            
            lRetVal = sl_Start(NULL,NULL,NULL);
            WAC_ASSERT_ON_ERROR(lRetVal);

            // Wait for device (STA) to connect to an AP
            while (!(statusWlan & STATUS_WLAN_STA_CONNECTED))
            {
                lRetVal = sl_DevGet(SL_DEVICE_GET_STATUS,&ConfigOpt,\
                                    &ConfigLen,(unsigned char *)(&statusWlan));
                WAC_ASSERT_ON_ERROR(lRetVal);

                sl_ExtLib_Time_Delay(7); //Grace period to finish initialization            
                if (timeoutCtr++ >  WAC_TIMEOUT)
                {
                    return EXTLIB_WAC_ERROR_IP_ACQ_FAILED;
                }
            }
            timeoutCtr = 0;
            
            while (ipV4.ipV4 == 0)
            {
                lRetVal = sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&dhcpIsOn,&len,(unsigned char *)&ipV4);
                WAC_ASSERT_ON_ERROR(lRetVal);
                
                sl_ExtLib_Time_Delay(7); //Grace period to finish initialization            
                if (timeoutCtr++ >  WAC_TIMEOUT)
                {
                    return EXTLIB_WAC_ERROR_IP_ACQ_FAILED;
                }
            }
            
        }
        else
        {
            //ARP Response received. IP address conflict. Try new IP address
            ulRandumNum++;
        }
    }
    return EXTLIB_WAC_SUCCESS;
}

#endif



//****************************************************************************
//
//! This function is to initialize WAC data structure
//!
//! \param[out] WAC data structure
//!
//! \return success:0, else:error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_initWacData()
{
    //unsigned char  deviceURNLen = sizeof(g_pWACData->credentialsStructure.name);
	unsigned char  deviceURNLen = strlen((char *)g_CurrDevURNname) + 1; // 1 for NULL
    signed long lRetVal = -1;

    // Reset TLV parameters that i'm getting from the configuring device  
    memset(&g_WAC_GlobalData, 0, sizeof(g_WAC_GlobalData));
    memset(g_pWACData, 0, sizeof(g_pWACData));

#if 0
    if(g_WACUserData.wacDeviceURNSize)
    {

    	strcpy((char*)g_pWACData->credentialsStructure.name, (const char*)g_WACUserData.wacDeviceURN);
    	g_pWACData->credentialsStructure.nameLen = g_WACUserData.wacDeviceURNSize;
    }
#endif

    if(deviceURNLen > 1)
    {
    	strcpy((char*)g_pWACData->credentialsStructure.name, (const char*)g_CurrDevURNname);
    	g_pWACData->credentialsStructure.nameLen = deviceURNLen; // 1 for null char

    	// also set g_WACUserData.wacDeviceURN and g_WACUserData.wacDeviceURNSize
    	g_WACUserData.wacDeviceURN = malloc(deviceURNLen);
    	memcpy(g_WACUserData.wacDeviceURN, g_CurrDevURNname, deviceURNLen);
    	g_WACUserData.wacDeviceURNSize = deviceURNLen;

    	// also set the device name to NWP here.
    	lRetVal = sl_NetAppSet (SL_NETAPP_DEVICE_ID, SL_NETAPP_DEVICE_URN, \
    							g_pWACData->credentialsStructure.nameLen,\
								(unsigned char *)g_pWACData->credentialsStructure.name);
    	WAC_ASSERT_ON_ERROR(lRetVal);
    }
    else
    {
    	deviceURNLen = MAX_DEVICE_URN_LENGTH;
    	// Set TLV parameters that i'm sending the configuring device
    	lRetVal = sl_NetAppGet (SL_NETAPP_DEVICE_ID, \
    							SL_NETAPP_DEVICE_URN, \
                            &deviceURNLen, \
                            g_pWACData->credentialsStructure.name);
    	g_pWACData->credentialsStructure.nameLen = deviceURNLen;
    	WAC_ASSERT_ON_ERROR(lRetVal);
    }

    // Same as what's published in the information element
    memcpy(g_pWACData->credentialsStructure.firmwareRevision, SOURCEVER, \
            sizeof(SOURCEVER)); 

    if (g_WACUserData.wacModelSize == 0)
    {
    	// set to all zeros because wacModel  is not set by the application
        memcpy(g_pWACData->credentialsStructure.hardwareRevision, 0, \
                sizeof(g_pWACData->credentialsStructure.hardwareRevision));
    }
    else
    {
        //Same as what's published in the information element
        memcpy(g_pWACData->credentialsStructure.hardwareRevision, \
                g_WACUserData.wacModel, g_WACUserData.wacModelSize); 
    }
    return EXTLIB_WAC_SUCCESS;
}

//****************************************************************************
//
//! This function is to initialize WAC user data structure, MFI data
//!
//! \param[in] none
//!
//! \return success:0 , else error code
//
//****************************************************************************
static signed long _sl_ExtLib_WAC_initUserDataStructure()
{
    //Init user data structure
    g_WACUserData.wacUserDataSet = 0;
    g_WACUserData.flags = SL_WAC_AIRPRINT; 
    g_WACUserData.wacInfoApp = 0;
    g_WACUserData.wacLang = 0;
    g_WACUserData.wacMFiProt = 0;
    g_WACUserData.wacModel = 0;
    g_WACUserData.wacUserDataSet = 0;
    g_WACUserData.wacInfoAppSize = 0;
    g_WACUserData.wacLangSize = 0;
    g_WACUserData.wacMFiProtSize = 0;
    g_WACUserData.wacModelSize = 0;
    g_WACUserData.wacVendorSize = 0;

    memset(g_pMFiData->certificateData, 0, MFI_CERTIFICATE_LEN);
    g_pMFiData->certificateDataLength = 0;
    memset(g_pMFiData->challengeData, 0, MFI_CHALLENGE_DATA_LEN);
    g_pMFiData->challengeDataLength = 0;
    
    return EXTLIB_WAC_SUCCESS; 
}

//****************************************************************************
//
//! This function is to initialize network parameter and features
//!
//! \param[in] none
//!
//! \return success:0 , else error code
//
//****************************************************************************

static signed long _sl_ExtLib_WAC_networkInit()
{
    signed long lRetVal = -1;


    //unsigned int IfBitmap = 0; // used in enabling IPv6

    //
    // 1. get MAC address from device and update global variable
    // 2. delete all stored profiles
    // 3. Disable IPv4 
    // 4. enable DHCP client (for AP mode)
    // 5. stop WAC mDNS service only
    // 6. reset NWP
    //
    
     //lRetVal = sl_NetCfgGet(SL_MAC_ADDRESS_GET,NULL,&g_ucMacAddressLen,\
                            (unsigned char *)g_pMacAddress); // R1 SL
    lRetVal = sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET,NULL, (unsigned short *)&g_ucMacAddressLen,\
                                (unsigned char *)g_pMacAddress);  // R2 SL

    WAC_ASSERT_ON_ERROR(lRetVal);
     
     // Remove all profiles
    lRetVal = sl_WlanProfileDel(0xFF);
    WAC_ASSERT_ON_ERROR(lRetVal);
    
#if 0
    //Set Ip to 0
    //SlNetCfgIpV4Args_t ipV4 = {0};
    //lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_STATIC_ENABLE, IPCONFIG_MODE_DISABLE_IPV4, \
                sizeof(SlNetCfgIpV4Args_t),(unsigned char *)&ipV4) ;// R1 SL : Not doing this for R2

    WAC_ASSERT_ON_ERROR(lRetVal);
    
    // Enable DHCP client
    //lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal); //-----> for R1 SL
    lRetVal = sl_NetCfgSet(SL_NETCFG_IPV4_STA_ADDR_MODE,  SL_NETCFG_ADDR_DHCP_LLA, 0, NULL); //----> for R2 SL

    WAC_ASSERT_ON_ERROR(lRetVal);
    
    // enable IPV6
	// enable ipv6 local

#ifdef ENABLE_IPV6_GLOBAL_ADDR
	IfBitmap = SL_NETCFG_IF_IPV6_STA_LOCAL | SL_NETCFG_IF_IPV6_STA_GLOBAL;
#else
	IfBitmap = SL_NETCFG_IF_IPV6_STA_LOCAL;
#endif

	//lRetVal = sl_NetCfgSet(SL_NETCFG_IF, SL_NEFCFG_IF_STATE,sizeof(IfBitmap),(_u8 *)&IfBitmap); // R2 V29
	lRetVal = sl_NetCfgSet(SL_NETCFG_IF, SL_NETCFG_IF_STATE,sizeof(IfBitmap),(_u8 *)&IfBitmap); //R2 V32

	WAC_ASSERT_ON_ERROR(lRetVal);

	lRetVal = sl_NetCfgSet(SL_NETCFG_IPV6_ADDR_LOCAL, SL_NETCFG_ADDR_STATELESS, NULL, NULL);
	WAC_ASSERT_ON_ERROR(lRetVal);

#ifdef ENABLE_IPV6_GLOBAL_ADDR
	lRetVal = sl_NetCfgSet(SL_NETCFG_IPV6_ADDR_GLOBAL, SL_NETCFG_ADDR_STATEFUL, NULL, NULL);
	WAC_ASSERT_ON_ERROR(lRetVal);
#endif
#endif

    // stop WAC mDNS message broadcast only
    lRetVal = WAC_MDNS_STOP_SERVICE(); 
    WAC_ASSERT_ON_ERROR(lRetVal);
    
    lRetVal = sl_Stop(100);
    WAC_ASSERT_ON_ERROR(lRetVal);
    
    lRetVal = sl_Start(NULL,NULL,NULL);
    WAC_ASSERT_ON_ERROR(lRetVal);
    
    return EXTLIB_WAC_SUCCESS;   
}


//****************************************************************************
//
//! This function is to de-initialize WAC configurations
//!             - Close MFI I2C connection
//!             - stop existing mDNS service
//!             - reset AP parameters (beacons settings as per WAC)
//!
//! \param[in] none
//!
//! \return success:0 , else error code
//
//****************************************************************************
signed long sl_ExtLib_WacDeInit(void)
{
    signed long lRetVal = -1;
    
    // free WAC control buffer memory and MAC address
    WAC_MEM_FREE(g_pWACControlBuf);
    WAC_MEM_FREE(g_pMacAddress);
    WAC_MEM_FREE(g_pWACData);
    WAC_MEM_FREE(g_pMFiData);

    sl_ExtLib_MFi_Close();
  
    lRetVal = WAC_MDNS_STOP_SERVICE(); //Stop previous MDNS service
    WAC_ASSERT_ON_ERROR(lRetVal);

    //resetting beacon info in STA mode caused code crash in R2 SL v19
	// issue fixed in R2 SL v24. But doing it in AP mode is more logical.
	// So, resetting the beacon info in AP mode itself just before switching role to STA
    // moving back to resetting beacon here : R2 SL V29.
    // Reason: WAC failure at any point should have beacon reset.
     lRetVal = _sl_ExtLib_WAC_Reset_AP_Params();
     WAC_ASSERT_ON_ERROR(lRetVal);

     DBG_PRINT("\n\rResetting Apple IE in Beacon ");

    return lRetVal;
}

//****************************************************************************
//
//! This function is to initialize WAC configurations and data structures
//!             - open MFI I2C connection
//!             - init User data structure
//!             - init network configurations
//!
//! \param[in] MFI address
//! \param[in] device private key specified by app else WAC lib has it's own
//!
//! \return success:0 , else error code
//
//****************************************************************************
signed long sl_ExtLib_WacInit(unsigned char mfiAddress, const unsigned char *devicePrivateKey)
{
    signed long lStatus = 0;
      
    if(devicePrivateKey != NULL)
    {
        // assign dvice private key passed by Application
        memcpy(&g_ucOurPrivateKey[0], devicePrivateKey, WAC_PRIVATE_KEY_LEN);
    }
    
    g_pMFiData = malloc(sizeof(_sl_ExtLib_MFi_Data_t));
    if(g_pMFiData == NULL)
    {
       lStatus = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    memset(g_pMFiData, 0, sizeof(_sl_ExtLib_MFi_Data_t));
    
    g_pWACData = malloc(sizeof(_sl_ExtLib_WAC_Data_t));
    if(g_pWACData == NULL)
    {
        lStatus = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    memset(g_pWACData, 0, sizeof(_sl_ExtLib_WAC_Data_t));
    
    // initialize WAC control buffer
    g_pWACControlBuf = (unsigned char*) malloc(WLAN_PACKET_SIZE);
    if(g_pWACControlBuf == NULL)
    {
        lStatus = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    memset(g_pWACControlBuf, 0, WLAN_PACKET_SIZE);
    
    g_pMacAddress = (unsigned char*) malloc(SL_MAC_ADDR_LEN);
    if(g_pMacAddress == NULL)
    {
        lStatus = ETXLIB_WAC_ERROR_MEM_ALLOC;
        goto _error_exit;
    }
    
    lStatus = sl_ExtLib_MFi_Open (mfiAddress, MFI_I2C_MASTER_MODE_FST); //Need to reconsider how i'm passing the I2C speed

    if(lStatus < 0) 
    {    
        DBG_PRINT("Failed to communicate with Authentication Coprocessor!!!\n\r");
        g_WAC_GlobalData.Initialized = 0;
        lStatus = EXTLIB_WAC_ERROR_MFI_COMMUNICATION;
        goto _error_exit;
    }

    g_WACUserData.wacPortNo = EXTLIB_WAC_PORT;
    
    _sl_ExtLib_WAC_initUserDataStructure();
    
    g_WAC_GlobalData.Initialized = 1;
    DBG_PRINT("Found Authentication Coprocessor Revision 2.0C\n\r");

    goto _success_exit;
    
_error_exit:
    // Free the memory in case of error case
    WAC_MEM_FREE(g_pWACControlBuf);
    WAC_MEM_FREE(g_pMacAddress);
    return lStatus;
    
_success_exit:    
      return EXTLIB_WAC_SUCCESS;
}


//****************************************************************************
//
//! This function is to start WAC state machine and follow steps to provision 
//!                     the device using WAC protocol
//!
//! \param[in] flag
//!
//! \return success:0 , else error code
//
//****************************************************************************
signed long sl_ExtLib_WacRun(unsigned char ucFlags)
{
    signed long Status, size, responseSize=0;
    _sl_ExtLib_WAC_IE_t pWacIE;
    signed long lRetVal = 0; 
    signed short WACListeningSockID = 0;
    signed short WACControlSockID = -1;
    signed long lSimplelinkRole = 0;
    volatile unsigned long timeoutCtr=0;


    unsigned short int IpV4_len = sizeof(SlNetCfgIpV4Args_t);
    unsigned short int dhcpIsOn;
    SlNetCfgIpV4Args_t ipV4 = {0};

    unsigned short int IpV6_len = sizeof(SlNetCfgIpV6Args_t);
    unsigned short int alloc_type;
    SlNetCfgIpV6Args_t ipV6 = {0};


    // TEMP VARIABLES FOR GET_STATUS --> to detect AP connection
    g_pWACData->deviceConfigured = 0;
    g_pWACData->WACDone = 0;
    SL_WAC_WACState = wsInit;

    //SlSecParams_t secParams; //R2 v24
    SlWlanSecParams_t secParams; //R2 v29
  
    //TODO add check if device is currently in AP mode if not return EXTLIB_WAC_ERROR_NOT_IN_AP_MODE
    
    if(g_pWACControlBuf == NULL)
    {
        WAC_ASSERT_ON_ERROR(ETXLIB_WAC_ERROR_MEM_ALLOC);
    }
    

    lRetVal = _sl_ExtLib_WAC_networkInit();
    if(lRetVal < 0)
    {
        return EXTLIB_WAC_ERROR_BY_NWP;
    }

#if 0
    lRetVal = sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET,NULL, (unsigned short *)&g_ucMacAddressLen,\
                                    (unsigned char *)g_pMacAddress);  // R2 SL
    WAC_ASSERT_ON_ERROR(lRetVal);

    // stop WAC mDNS message broadcast only: check if this is really required
	lRetVal = WAC_MDNS_STOP_SERVICE();
	WAC_ASSERT_ON_ERROR(lRetVal);
#endif

    while (!g_pWACData->WACDone)
    {
        switch (SL_WAC_WACState)
        {
            case (wsInit):
            {
                signed short    AddrSize;
                SlSockAddrIn_t  Addr;
    
                //Check that the WAC init function has ended successfully
                if (g_WAC_GlobalData.Initialized == 0) 
                {
                    return EXTLIB_WAC_ERROR_UNINITIALIZED;
                }

                lRetVal = _sl_ExtLib_WAC_initWacData();
                // return if WAC data structure init failed
                WAC_ASSERT_ON_ERROR(lRetVal);
                
                DBG_PRINT("Get MFI certificate\n\r");

                g_pMFiData->certificateDataLength = sl_ExtLib_MFi_Get(MFI_GET_CERTIFICATE, \
                                                   0, 0, \
                                                   &g_pMFiData->certificateData[0], \
                                                   sizeof(g_pMFiData->certificateData));
                // return if failed to get MFI certificate
                WAC_ASSERT_ON_ERROR(g_pMFiData->certificateDataLength);
                
                DBG_PRINT("Applying Info Element\n\r");
                lRetVal = _sl_ExtLib_WAC_Build_IE(&pWacIE);
                // return if failed to Build WAC IE
                WAC_ASSERT_ON_ERROR(lRetVal);
                
                //Initialize WAC IE structure  
                lRetVal = _sl_ExtLib_WAC_Set_AP_Params(&pWacIE);
                // return if WAC set AP params failed
                WAC_ASSERT_ON_ERROR(lRetVal);
                
                // Start information element broadcast
                DBG_PRINT("Applying mDNS Service\n\r");
                lRetVal = WAC_MDNS_START_SERVICE(0); //Start MDNS broadcast
                // return if WAC mDNS failed to start
                WAC_ASSERT_ON_ERROR(lRetVal);
                
                DBG_PRINT("Opening socket for port %d. Waiting for "\
                           "configuring device connection...\r\n", g_WACUserData.wacPortNo);
                Status = _sl_ExtLib_WAC_Init_Socket(&WACListeningSockID, ROLE_AP);
                if (Status != 0)
                {
                    WAC_ASSERT_ON_ERROR(Status); //In case of error, return it
                }

                //wait for connection
                while (WACControlSockID < 0)
                {
                    WACControlSockID = sl_Accept(WACListeningSockID, \
                                                  (struct SlSockAddr_t *)&Addr,\
                                                  (SlSocklen_t*)&AddrSize);
                    //Delay 1msec
                    sl_ExtLib_Time_Delay(10);
                    timeoutCtr += 10;
                    if(timeoutCtr > WAC_NO_CONNECT_TIMEOUT)
                    {
                        DBG_PRINT("No WAC Connection for 30 Minutes.. Terminating \n\r");

                        //Close Listening Socket
                        lRetVal = _sl_ExtLib_WAC_Close_Socket(WACListeningSockID);               
                        WAC_ASSERT_ON_ERROR(lRetVal);
                        
                        WAC_ASSERT_ON_ERROR(EXTLIB_WAC_ERROR_TIMEOUT);
                    }
                }
                timeoutCtr = 0;
                DBG_PRINT("Connection established with configuring device\n\r");
                SL_WAC_WACState = wsCredentials;
            
                break;
            }

            case (wsCredentials):
            {
                Status = -1;
                unsigned char clientPublicKey[32];
                
                while (Status < 0) 
                {
                    // Receive control packet 
                    Status = sl_Recv(WACControlSockID, g_pWACControlBuf, \
                                    WLAN_PACKET_SIZE, 0);
                    // Parse command, Only if we got something in the last sl_Recv    
                    if (Status > 0) 
                    {
                        switch (g_pWACControlBuf[0])
                        {
                            case 'P': //Post
                                //Extract client public key from first post message
                                size = WACParsePost(g_pWACControlBuf, &clientPublicKey[0]);
                                if (size < 0)
                                {
                                    //Could not locate key in the received packet
                                    WAC_ASSERT_ON_ERROR(EXTLIB_WAC_ERROR_BAD_PACKET);
                                }
                                responseSize = _sl_ExtLib_WAC_rtsp_BuildPostResponse(g_pWACControlBuf, SETUP_RESPONSE, \
                                                                                      Status, &clientPublicKey[0], size, \
                                                                                      g_pWACData, g_pMFiData->certificateDataLength, \
                                                                                      &g_pMFiData->certificateData[0]);
                                WAC_ASSERT_ON_ERROR(responseSize);
                                
                                //Send response       
                                lRetVal = sl_Send(WACControlSockID, g_pWACControlBuf, responseSize, 0); 
                                // return if failed to send reponse back to iDevice
                                WAC_ASSERT_ON_ERROR(lRetVal);
                                
                                DBG_PRINT("Received keys from configuring device, sent response of %d bytes\n\r", responseSize);
                                SL_WAC_WACState = wsGetTLV;
                                break;

                            default: 
                                DBG_PRINT("Warning - recieved unexpected packet while expecting public key - %s\n\r", g_pWACControlBuf);
                                break;   
                        } 
                    } 
                    sl_ExtLib_Time_Delay(10); //Yield for some time
                }
                break;
            }
            case (wsGetTLV):
            {  
                Status = -1;
                
                while (Status < 0) 
                {
                    // Receive control packet 
                    Status = sl_Recv(WACControlSockID, g_pWACControlBuf, WLAN_PACKET_SIZE, 0);
                  
                    // Parse command 
                    if (Status > 0) //Only if we got something in the last sl_Recv
                    {
                        switch (g_pWACControlBuf[0])
                        {
                            case 'P': //Post
                                //Extract TLV information from second Post message
                                size = _sl_ExtLib_WAC_GetTLV(g_pWACControlBuf, g_pWACData); 

                                if (size < 0)
                                {
                                    DBG_PRINT("Error getting TLV Data. Error code %d\n\r", size);
                                }
                                else 
                                {
                                    DBG_PRINT("Got TLV Data, %d bytes\n\r", size);
                                    //Prase decrypted TLV Data
                                    size = _sl_ExtLib_WAC_ParseTLV(g_pWACData); 
                                }

                                if (size < 0)
                                {
                                  //TODO if this error comes then what to do next??
                                    DBG_PRINT("Error parsing TLV - unknown parameter encountered\n\r");
                                }
                                else
                                {
                                    DBG_PRINT("Requested SSID is %s\n\r", \
                                        g_pWACData->credentialsStructure.wifiSSID);
                                    
                                    DBG_PRINT("Requested Name for Accessory is %s\n\r", \
                                        &g_pWACData->credentialsStructure.name[0]);
                                    if (g_pWACData->credentialsStructure.keyExists)
                                    {
                                        DBG_PRINT("Target network is password protected\n\r");
                                    }
                                    else
                                    {
                                        DBG_PRINT("Target network is not password protected\n\r");
                                    }
                                }

                                responseSize = _sl_ExtLib_WAC_rtsp_BuildConfigResponse (g_pWACControlBuf, CONFIG_RESPONSE, g_pWACData);
                                lRetVal = sl_Send(WACControlSockID, g_pWACControlBuf, responseSize, 0); //Send response       
                                if(lRetVal < 0)
                                {
                                    WAC_ASSERT_ON_ERROR(EXTLIB_WAC_ERROR_SEND_DATA_FAILED);
                                }
                                DBG_PRINT("Registered new device name\n\r");
                                DBG_PRINT("Sent TLV response\n\r");
                                SL_WAC_WACState = wsWaitSocketClose;
                                break;

                            default: //By default - do nothing.
                                DBG_PRINT("Warning - recieved unexpected packet while expecting TLV information: %s\n\r", g_pWACControlBuf);
                                break;   
                        } 
                  }
                  else if (Status == 0)
                  {
                    DBG_PRINT("Error - other side has closed the connection, meaning key exchange has failed! Attempting again...\n\r");
                    SL_WAC_WACState = wsInit;
                  }          
                 sl_ExtLib_Time_Delay(10); //Yield for some time
            }        
            break;
        }
        case (wsWaitSocketClose):
        {
            Status = -1;
        
            while (Status < 0) 
            {
              // Check when socket is closed by the other side by doing dummy receives 
                Status = sl_Recv(WACControlSockID, g_pWACControlBuf, WLAN_PACKET_SIZE, 0);
                sl_ExtLib_Time_Delay(10); //Yield for some time

                if (Status == 0) //Connection has closed
                {
                    SL_WAC_WACState = wsApplyCredentials;
                    DBG_PRINT("Configuring device has closed the connection, as expected at this point\n\r");
                    sl_Close(WACControlSockID); //Close my connection
                    break;
                }
            }
            break;
        }
        case (wsApplyCredentials):
        {
            // SlSecParams_t secParams;
            
#if defined(sl_ExtLib_Get_WLAN_Status)
            do
            {
                g_ulWacNwStatus = sl_ExtLib_Get_WLAN_Status();
#ifndef SL_PLATFORM_MULTI_THREADED
                _SlNonOsMainLoopTask();
#endif
                sl_ExtLib_Time_Delay(10);
                if (timeoutCtr++ >  WAC_TIMEOUT*10)
                {
                    DBG_PRINT("FATAL ERROR - Timeout while waiting for iDevice disconnect event!\n\r");
                    break;
                }  
                
            }while(WAC_GET_NW_STATUS_BIT(g_ulWacNwStatus, WAC_SL_STATUS_BIT_CONNECTION));
            timeoutCtr = 0;
#else
            sl_ExtLib_Time_Delay(20);
#endif

            // sl_NetAppMDNSUnRegisterService(" ", 0); // R1 SL
            sl_NetAppMDNSUnRegisterService(" ", 0, 0 ); // R2 SL

            // adding resetting beacon information just before going to station mode. : fix for R2 SL v19
            // Moving this back to wacdeinit
            // Refer to comments in WAC deinit function
            // lRetVal = _sl_ExtLib_WAC_Reset_AP_Params();
            // WAC_ASSERT_ON_ERROR(lRetVal);

            DBG_PRINT("Setting station mode for next WLAN startup\n\r");
            lRetVal = sl_WlanSetMode(ROLE_STA);
            DBG_PRINT("Adding profile for SSID \'%s\' \n\r Password: \'%s\' \n\r", \
              g_pWACData->credentialsStructure.wifiSSID, g_pWACData->credentialsStructure.wifiPSK);       
            if (g_pWACData->credentialsStructure.keyExists)
            {
                //secParams.Type = SL_SEC_TYPE_WPA; // R2 V29
            	secParams.Type = SL_WLAN_SEC_TYPE_WPA; // R2 v32
                secParams.Key = (signed char*)g_pWACData->credentialsStructure.wifiPSK;
                secParams.KeyLen = g_pWACData->credentialsStructure.wifiPSKLen;
            }
            else
            {
                //secParams.Type = SL_SEC_TYPE_OPEN; // R2 v29
            	secParams.Type =  SL_WLAN_SEC_TYPE_OPEN; // R2 V32
                secParams.Key = 0;
                secParams.KeyLen = 0;
            }
       
            lRetVal = sl_WlanProfileAdd((signed char*)g_pWACData->credentialsStructure.wifiSSID,\
                                        g_pWACData->credentialsStructure.wifiSSIDLen,\
                                    0,&secParams,0,1,0);
            WAC_ASSERT_ON_ERROR(lRetVal);
            
            DBG_PRINT("Setting connection policy = auto connect \n\r");
            // Set AUTO policy 
            //lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, \
                                                    SL_CONNECTION_POLICY(1,0,0,0,0), 0, 0); // R1 SL
            //lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, \
                                        SL_CONNECTION_POLICY(1,0,0,0,0,0), 0, 0); // R2 SL v29
            lRetVal = sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, \
            							SL_WLAN_CONNECTION_POLICY(1,0,0,0,0,0), 0, 0); // R2 SL v29

            //DBG_PRINT("Setting connection policy = manual connect\n\r");
            // lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, \
                                                    SL_CONNECTION_POLICY(0,0,0,0,0,0), 0, 0); // R2 SL

            WAC_ASSERT_ON_ERROR(lRetVal);

            //Update device URN with new name
            lRetVal = WAC_MDNS_STOP_SERVICE(); //Stop previous MDNS service
            WAC_ASSERT_ON_ERROR(lRetVal);

            lRetVal = sl_NetAppSet (SL_NETAPP_DEVICE_ID, \
                                        SL_NETAPP_DEVICE_URN, \
                                        g_pWACData->credentialsStructure.nameLen, \
                                    (unsigned char *) g_pWACData->credentialsStructure.name);
            WAC_ASSERT_ON_ERROR(lRetVal);
            //update the global buffer holding the device name
            lRetVal = sl_ExtLib_DevURN_WacSet(g_pWACData->credentialsStructure.name, g_pWACData->credentialsStructure.nameLen);
            WAC_ASSERT_ON_ERROR(lRetVal);               
            
            DBG_PRINT("Shutting down WLAN\n\r");
            lRetVal = sl_Stop(200);
            g_pWACData->deviceConfigured=1; //Device is now configured
       
            SL_WAC_WACState = wsConnectStation;
            break;
        }
        case (wsConnectStation):
        {
            unsigned long statusWlan = 0;
            unsigned char ConfigOpt = SL_DEVICE_EVENT_CLASS_WLAN;
            unsigned short ConfigLen = 64;
            
            //lSimplelinkRole =  sl_Start(NULL,NULL,NULL); //sept 8

            //added this line for the flow of providing visual ip indication in station mode only
            // in async event: change relevent only to hk blink build.
            //g_currSimplelinkRole = lSimplelinkRole;

            sl_Start_custom(); // this call will update g_currSimplelinkRole; //sept_8
            lSimplelinkRole = g_currSimplelinkRole; // updating lSimplelinkRole to not break teh flow //sept 8
            
            if (lSimplelinkRole == ROLE_AP)
            {
                DBG_PRINT("Fatal error - restarted as AP!\n\r");
                WAC_ASSERT_ON_ERROR(ERROR_RESTARTED_IN_AP_MODE);
            }
            if (lSimplelinkRole == ROLE_STA)
            {
            	g_ipv6_acquired = 0;
            	g_ipv4_acquired = 0;

                DBG_PRINT("WLAN restarted as station\n\r");

#if 0
                // doing manual connect
                DBG_PRINT("Giving Command to connect to AP \n\r");
				lRetVal = sl_WlanConnect((signed char*)g_pWACData->credentialsStructure.wifiSSID, g_pWACData->credentialsStructure.wifiSSIDLen, 0, &secParams, 0); //------------> debug
				WAC_ASSERT_ON_ERROR(lRetVal);
				DBG_PRINT("Command given to connect to AP \n\r");
#endif

				// Waiting for the device to connect to target AP
                while (!(statusWlan & SL_DEVICE_STATUS_WLAN_STA_CONNECTED))
                {
                    lRetVal = sl_DeviceGet(SL_DEVICE_GET_STATUS,&ConfigOpt,\
                	                                        &ConfigLen,(unsigned char *)(&statusWlan));
                    WAC_ASSERT_ON_ERROR(lRetVal);
                    
                    sl_ExtLib_Time_Delay(1);
                    
                    if (timeoutCtr++ >  WAC_AP_CONNECT_TIMEOUT)
                    {
                        DBG_PRINT("FATAL ERROR - Timeout while waiting for connection to target AP!\n\r");
                        WAC_ASSERT_ON_ERROR(EXTLIB_WAC_ERROR_TIMEOUT);             
                    }
                }

                timeoutCtr = 0;
                DBG_PRINT("Connected to AP : Waiting for device to acquire IPs \n\r");
                
                // Waiting for DHCP done 

                //should ipv4 be made zero here?
#if 1
                while ((ipV4.Ip == 0) || (ipV6.Ip == 0))
                {
                    //lRetVal = sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&dhcpIsOn,\
                                                &len,(unsigned char *)&ipV4); // R1 SL
                    //lRetVal = sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE, &dhcpIsOn,\
                                                &len,(unsigned char *)&ipV4); // R2 SL
                    lRetVal = sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE, &dhcpIsOn,\
                                           &IpV4_len,(unsigned char *)&ipV4); // R2 SL
                    WAC_ASSERT_ON_ERROR(lRetVal);

                    lRetVal = sl_NetCfgGet(SL_NETCFG_IPV6_ADDR_LOCAL, &alloc_type, &IpV6_len, (_u8 *)&ipV6);
                    WAC_ASSERT_ON_ERROR(lRetVal);

#else

               // Wait till both IPV4 and IPv6 are acquired
               while ((g_ipv4_acquired == 0) || (g_ipv6_acquired == 0))
               {
#endif
                    sl_ExtLib_Time_Delay(1);
                    
#ifndef SL_PLATFORM_MULTI_THREADED
				//todo nope sorry _SlNonOsMainLoopTask();
#endif

                    if (timeoutCtr++ >  WAC_DHCP_TIMEOUT)
                    {
#ifndef SL_LLA_SUPPORT                
                        DBG_PRINT("FATAL ERROR - Timeout while waiting for DHCP done on target AP!\n\r");
                        return EXTLIB_WAC_ERROR_TIMEOUT; 
#else
                        DBG_PRINT("ERROR - Timeout DHCP Timeout - Assign IP using LLA!\n\r");
                        if(_sl_ExtLib_WAC_ConnectUsingLLA() < 0)
                        {
                            DBG_PRINT("FATAL ERROR - Couldn't Assign IP using LLA!\n\r");
                            WAC_ASSERT_ON_ERROR(EXTLIB_WAC_ERROR_TIMEOUT); 
                        }
                        else
                        {
                            DBG_PRINT("LLA IP Assignment Done!\n\r");
                            break;
                        }
#endif            
                          
                    }          
                }// end of while
                timeoutCtr = 0;

			   //DBG_PRINT("\n\r IP Acquired[%d.%d.%d.%d] when connected to WAC configured AP!\n\r",SL_IPV4_BYTE(ipV4.Ip,3),\
                                      SL_IPV4_BYTE(ipV4.Ip,2),SL_IPV4_BYTE(ipV4.Ip,1),SL_IPV4_BYTE(ipV4.Ip,0));
			   //DBG_PRINT("\n\r DHCP ON = %d \n\r", dhcpIsOn);

                DBG_PRINT("\n\r Connected to %s, applying new MDNS service with Seed=1 and new URN\n\r", g_pWACData->credentialsStructure.wifiSSID);
                DBG_PRINT("\n\r Registering new device URN - %s \n\r", g_pWACData->credentialsStructure.name);
                DBG_PRINT("\n\r Waiting for configuring device on %s. This may take a few seconds...\n\r", g_pWACData->credentialsStructure.wifiSSID);
                

#if 0
                //Set new URN as SSID
                lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, \
                                          g_pWACData->credentialsStructure.nameLen, \
                                      g_pWACData->credentialsStructure.name); 
                WAC_ASSERT_ON_ERROR(lRetVal);
#endif

                //Restart MDNS service, with Seed=1 (to signal we're configured)
                lRetVal = WAC_MDNS_START_SERVICE(1); 
                WAC_ASSERT_ON_ERROR(lRetVal);

                lRetVal = _sl_ExtLib_WAC_Init_Socket(&WACListeningSockID, ROLE_STA);
                WAC_ASSERT_ON_ERROR(lRetVal);

                SL_WAC_WACState = wsWaitForNewConnection;
            }
            break;
        }
        case (wsWaitForNewConnection):
        {
            signed short    AddrSize;
            SlSockAddrIn_t  Addr;
    
            WACControlSockID = -1;
        
            //wait until connection
            while (WACControlSockID < 0)
            {
                WACControlSockID = sl_Accept(WACListeningSockID, \
                                                (struct SlSockAddr_t *)&Addr, \
                                                (SlSocklen_t*)&AddrSize);
                sl_ExtLib_Time_Delay(10);

                if (timeoutCtr++ >  WAC_ACK_RECV_TIMEOUT)
                {
                    DBG_PRINT("FATAL ERROR - Timeout while waiting for socket connection from iDevice via target AP!\n\r");
                    WAC_ASSERT_ON_ERROR(EXTLIB_WAC_ERROR_TIMEOUT);             
                }                   
            }        
            timeoutCtr = 0;
            DBG_PRINT("Connection established with configuring device on target AP\n\r");
            SL_WAC_WACState = wsFinalizeConfiguration;
            break;
        }
        case (wsFinalizeConfiguration):
        {      
            Status = -1;
        
            while (Status < 0) 
            {
                // Receive control packet 
                Status = sl_Recv(WACControlSockID, g_pWACControlBuf, WLAN_PACKET_SIZE, 0);
                switch (g_pWACControlBuf[0])
                {
                    case 'P': //Post
                        Status = memcmp(&g_pWACControlBuf[0], POST_CONFIGURED, (sizeof(POST_CONFIGURED) -1));
                        if (Status == 0) 
                        {
                            responseSize = _sl_ExtLib_WAC_rtsp_BuildConfiguredResponse (g_pWACControlBuf, CONFIGURED_RESPONSE);
                            lRetVal = sl_Send(WACControlSockID, g_pWACControlBuf, responseSize, 0); //Send response               
                            WAC_ASSERT_ON_ERROR(lRetVal);
                            
                            lRetVal = WAC_MDNS_STOP_SERVICE();
                            WAC_ASSERT_ON_ERROR(lRetVal);
                        }
                        else
                        {
                            Status = -1; //Not our packet, try again
                        }
                        break;

                    default: //By default - do nothing
                        break;   
                } 
          
            }
            
            DBG_PRINT("Recieved /configured message from configuring device\n\r");
            DBG_PRINT("Sent Response for /configured message\n\r");
            DBG_PRINT("Closing socket\n\r");
            sl_Close(WACControlSockID);
            DBG_PRINT("**************************\n\r");
            DBG_PRINT("         D O N E !        \n\r");
            DBG_PRINT("**************************\n\r");
        
        
            g_pWACData->WACDone = 1; //Exit statemachine
            break;
        }
        default:
            DBG_PRINT("Error - unknown state in state machine!\n\r");
            return ERROR_UNKNOWN_STATE;
        } //SM Switch
    } //While device is not configured            
  
  return EXTLIB_WAC_SUCCESS;
}



//*****************************************************************************
// Adding APIs to read and write the device URN name to a buffer in WAC lib
// The string in this buffer is used as the device URN name for mDNS WAC
// The strings are retained even after WAC is run, in the buffer accessed
// through these APIs. The same name can be used to for other services,
// after WAC completes.
// sl_ExtLib_WacSetDevURN  should be called before running WAC module.
//*****************************************************************************

int sl_ExtLib_DevURN_WacSet(unsigned char* pValue, unsigned short Length)
{
	if((pValue == NULL) || (Length <= 0) || (Length >= MAX_DEVICE_URN_LENGTH))
		return (EXTLIB_WAC_ERROR_INVALID_DEVICE_URN);

	memcpy(&g_CurrDevURNname , pValue, (Length+1) ); // 1 is for the null character
	return(EXTLIB_WAC_SUCCESS);
}

int sl_ExtLib_DevURN_WacGet(unsigned char* pValue, unsigned short *Length)
{
	int URNlen = strlen((char *)g_CurrDevURNname);

	if((pValue == NULL) || (*Length <= 0) || (*Length <= URNlen))
		return (EXTLIB_WAC_ERROR_INADEQUATE_BUF_LEN);

	memcpy( pValue, &g_CurrDevURNname, (URNlen+1));
	*Length = URNlen;

	return(EXTLIB_WAC_SUCCESS);
}


