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


#ifndef __MFI_USER_H__
#define __MFI_USER_H__

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
  
  
#define SL_MFI_SWAP16(x)                   ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))


#if defined(sl_ExtLib_I2C_Open)
extern signed long sl_ExtLib_I2C_Open(unsigned long ulMode);
#endif


#if defined(sl_ExtLib_I2C_Read)
extern signed long sl_ExtLib_I2C_Read(unsigned char ucDevAddr,
            unsigned char *pucData,
            unsigned char ucLen);
#endif

#if defined(sl_ExtLib_I2C_Write)
extern signed long sl_ExtLib_I2C_Write(unsigned char ucDevAddr,
             unsigned char *pucData,
             unsigned char ucLen, 
             unsigned char ucStop);
#endif


#if defined(sl_ExtLib_I2C_Close)
extern signed long sl_ExtLib_I2C_Close();
#endif


#if defined(sl_ExtLib_Time_Delay)
extern void sl_ExtLib_Time_Delay(unsigned long ulDelay);
#endif
  

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif 
