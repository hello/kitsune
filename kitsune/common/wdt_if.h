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

#ifndef __WDTIF_H__
#define __WDTIF_H__

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
// Typedef that can be passed to InitializeGPTOneShot as the fAPPDevCallbk 
// parameter.
//
//*****************************************************************************
typedef void (*fAPPWDTDevCallbk)();

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern void WDT_IF_Init(fAPPWDTDevCallbk fpAppWDTCB,
                          unsigned int uiReloadVal);
extern void WDT_IF_DeInit();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __WDTIF_H__
