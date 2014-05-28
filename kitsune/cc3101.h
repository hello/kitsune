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
// cc3101.h - Defines and Macros for 3101
//
//*****************************************************************************

#ifndef __CC3101_H__
#define __CC3101_H__

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

#include <FreeRTOS.h>
#include <task.h>
#include <datatypes.h>

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

//*****************************************************************************
// CC3000 State Machine Definitions
//*****************************************************************************
enum cc3101StateEnum
{
    CC3101_UNINIT           = 0x01, // CC3101 Driver Uninitialized
    CC3101_INIT             = 0x02, // CC3101 Driver Initialized
    CC3101_ASSOC            = 0x04, // CC3101 Associated to AP
    CC3101_IP_ALLOC         = 0x08, // CC3101 has IP Address
    CC3101_SERVER_INIT      = 0x10, // CC3101 Server Initialized
    CC3101_CLIENT_CONNECTED = 0x20  // CC3101 Client Connected to Server
};

  
//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern void InitCallBack(UINT32 Status);
extern void SimpleLinkWlanEventHandler(void* pArgs);
extern void SimpleLinkNetAppEventHandler(void* pArgs);
extern void SimpleLinkHttpServerCallback(void *pHttpServerEvent, void *pHttpServerResponse);
extern void InitDriver(void);
extern void DeInitDriver(void);
extern int GetCmd(char *pcBuffer, unsigned int uiBufLen);
extern int ConnectUsingSSID(char * pcSsid);
extern int IpConfigGet(unsigned long *aucIP, unsigned long *aucSubnetMask,
                unsigned long *aucDefaultGateway, unsigned long *aucDNSServer);
extern void UnsetCC3101MachineState(char stat);
extern void SetCC3101MachineState(char stat);
extern void ResetCC3101StateMachine();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif //  __CC3101_H__
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************


