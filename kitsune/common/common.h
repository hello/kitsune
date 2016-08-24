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

#ifndef __COMMON__H__
#define __COMMON__H__


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


//
// Values for below macros shall be modified as per access-point(AP) properties
// SimpleLink device will connect to following AP when application is executed
//
#define SSID_NAME           "cc3220demo"    /* AP SSID */
#define SECURITY_TYPE       SL_WLAN_SEC_TYPE_OPEN /* Security type (OPEN or WEP or WPA*/
#define SECURITY_KEY        ""              /* Password of the secured AP */
#define SSID_LEN_MAX        32
#define BSSID_LEN_MAX       6

#define SSID_AP_MODE		"<ap-ssid>"
#define SEC_TYPE_AP_MODE	SL_WLAN_SEC_TYPE_OPEN
#define PASSWORD_AP_MODE	""

//
// Values for below macros shall be modified per the access-point's (AP)
// properties.
// SimpleLink device will connect to following AP when the application is
// executed.
//
#define ENT_SSID_NAME		"externalhotspot84"
#define ENT_USERNAME		""
#define ENT_PASSKEY			""
#define ENT_SEC_METHOD		SL_WLAN_ENT_EAP_METHOD_PEAP0_MSCHAPv2
#define ENT_SEC_TYPE		SL_WLAN_SEC_TYPE_WPA_ENT


#ifdef NOTERM
#define UART_PRINT(x,...)
#define DBG_PRINT(x,...)
#define ERR_PRINT(x)
#else
#define UART_PRINT Report
#define DBG_PRINT  Report
#define ERR_PRINT(x) Report("Error [%d] at line [%d] in function [%s]  \n\r",x,__LINE__,__FUNCTION__)
#endif

// Loop forever, user can change it as per application's requirement
#define LOOP_FOREVER() \
            {\
                while(1); \
            }

// check the error code and handle it
#define ASSERT_ON_ERROR(error_code)\
            {\
                 if(error_code < 0) \
                   {\
                        return error_code;\
                 }\
            }

#define SPAWN_TASK_PRIORITY     9
#define SL_STOP_TIMEOUT         200
#define UNUSED(x)               ((x) = (x))
#define SUCCESS                 0
#define FAILURE                 -1


// Status bits - These are used to set/reset the corresponding bits in 
// given variable
typedef enum{
    STATUS_BIT_NWP_INIT = 0, // If this bit is set: Network Processor is 
                             // powered up
                             
    STATUS_BIT_CONNECTION,   // If this bit is set: the device is connected to 
                             // the AP or client is connected to device (AP)
                             
    STATUS_BIT_IP_LEASED,    // If this bit is set: the device has leased IP to 
                             // any connected client

    STATUS_BIT_IP_ACQUIRED,   // If this bit is set: the device has acquired an IP
    
    STATUS_BIT_SMARTCONFIG_START, // If this bit is set: the SmartConfiguration 
                                  // process is started from SmartConfig app

    STATUS_BIT_P2P_DEV_FOUND,    // If this bit is set: the device (P2P mode) 
                                 // found any p2p-device in scan

    STATUS_BIT_P2P_REQ_RECEIVED, // If this bit is set: the device (P2P mode) 
                                 // found any p2p-negotiation request

    STATUS_BIT_CONNECTION_FAILED, // If this bit is set: the device(P2P mode)
                                  // connection to client(or reverse way) is failed

    STATUS_BIT_PING_DONE,         // If this bit is set: the device has completed
                                 // the ping operation

	STATUS_BIT_IPV6L_ACQUIRED,   // If this bit is set: the device has acquired an IPv6 address
	STATUS_BIT_IPV6G_ACQUIRED,   // If this bit is set: the device has acquired an IPv6 address
	STATUS_BIT_AUTHENTICATION_FAILED,
	STATUS_BIT_RESET_REQUIRED,
}e_StatusBits;


#define CLR_STATUS_BIT_ALL(status_variable)  (status_variable = 0)
#define SET_STATUS_BIT(status_variable, bit) status_variable |= (1<<(bit))
#define CLR_STATUS_BIT(status_variable, bit) status_variable &= ~(1<<(bit))
#define CLR_STATUS_BIT_ALL(status_variable)   (status_variable = 0)
#define GET_STATUS_BIT(status_variable, bit) (0 != (status_variable & (1<<(bit))))

#define IS_NW_PROCSR_ON(status_variable)     GET_STATUS_BIT(status_variable,\
                                                            STATUS_BIT_NWP_INIT)
#define IS_CONNECTED(status_variable)        GET_STATUS_BIT(status_variable,\
                                                         STATUS_BIT_CONNECTION)
#define IS_IP_LEASED(status_variable)        GET_STATUS_BIT(status_variable,\
                                                           STATUS_BIT_IP_LEASED)
#define IS_IP_ACQUIRED(status_variable)       GET_STATUS_BIT(status_variable,\
                                                          STATUS_BIT_IP_ACQUIRED)
#define IS_IPV6L_ACQUIRED(status_variable)       GET_STATUS_BIT(status_variable,\
														STATUS_BIT_IPV6L_ACQUIRED)
#define IS_IPV6G_ACQUIRED(status_variable)       GET_STATUS_BIT(status_variable,\
														STATUS_BIT_IPV6G_ACQUIRED)


#define IS_SMART_CFG_START(status_variable)  GET_STATUS_BIT(status_variable,\
                                                   STATUS_BIT_SMARTCONFIG_START)
#define IS_P2P_DEV_FOUND(status_variable)    GET_STATUS_BIT(status_variable,\
                                                       STATUS_BIT_P2P_DEV_FOUND)
#define IS_P2P_REQ_RCVD(status_variable)     GET_STATUS_BIT(status_variable,\
                                                    STATUS_BIT_P2P_REQ_RECEIVED)
#define IS_CONNECT_FAILED(status_variable)   GET_STATUS_BIT(status_variable,\
                                                   STATUS_BIT_CONNECTION_FAILED)
#define IS_PING_DONE(status_variable)        GET_STATUS_BIT(status_variable,\
                                                           STATUS_BIT_PING_DONE)

#define IS_AUTHENTICATION_FAILED(status_variable)   GET_STATUS_BIT(status_variable,\
														STATUS_BIT_AUTHENTICATION_FAILED)


void NotifyReturnToFactoryImage();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif //__COMMON__H__

