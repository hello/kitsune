/******************************************************************************
*
*   Copyright (C) 2013 Texas Instruments Incorporated
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
******************************************************************************/

//*****************************************************************************
//
// i2c_if.h - Defines and Macros for the I2C interface.
//
//*****************************************************************************

#ifndef __I2C_IF_H__
#define __I2C_IF_H__

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

#define I2C_MRIS_CLKTOUT        0x2
//*****************************************************************************
//
// I2C transaction timeout value. 
// Set to value 0x7D. (@100KHz it is 20ms, @400Khz it is 5 ms)
//
//*****************************************************************************
#define I2C_TIMEOUT_VAL         0x7D

//*****************************************************************************
//
// Values that can be passed to I2COpen as the ulMode parameter.
//
//*****************************************************************************
#define I2C_MASTER_MODE_STD     0
#define I2C_MASTER_MODE_FST     1

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern int I2COpen(unsigned long ulMode);
extern int I2CClose();
extern int I2CWrite(unsigned char ucDevAddr,
             unsigned char *pucData,
             unsigned char ucLen, 
             unsigned char ucStop);
extern int I2CRead(unsigned char ucDevAddr,
            unsigned char *pucData,
            unsigned char ucLen);
extern int I2CReadFrom(unsigned char ucDevAddr,
            unsigned char *pucWrDataBuf,
            unsigned char ucWrLen,
            unsigned char *pucRdDataBuf,
            unsigned char ucRdLen);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  __I2C_IF_H__
