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
// network_if.h
//
// Networking interface macro and function prototypes for CC3220 device
//
//*****************************************************************************

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



//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern long Network_IF_InitDriver(unsigned int uiMode);
extern long Network_IF_DeInitDriver(void);
extern long Network_IF_ConnectAP(char * pcSsid, SlWlanSecParams_t SecurityParams);
extern long Network_IF_DisconnectFromAP();
extern long Network_IF_IpConfigGet(unsigned long *aucIP, unsigned long *aucSubnetMask,
                unsigned long *aucDefaultGateway, unsigned long *aucDNSServer);
extern long Network_IF_GetHostIP( char* pcHostName,unsigned long * pDestinationIP);
extern unsigned long Network_IF_CurrentMCUState();
extern void Network_IF_UnsetMCUMachineState(char stat);
extern void Network_IF_SetMCUMachineState(char stat);
extern void Network_IF_ResetMCUStateMachine();
extern unsigned short itoa(short cNum, char *cString);
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif


