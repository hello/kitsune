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


#ifndef __MFILIB_H__
#define __MFILIB_H__


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
  

#define MFI_I2C_MASTER_MODE_STD     0
#define MFI_I2C_MASTER_MODE_FST     1
  
  /* Error codes */
#define EXTLIB_MFI_ERROR_OK                     0
#define EXTLIB_MFI_ERROR_INVALID_ADDRESS        -1 //MFi Chip can either have address 0x10 or 0x11, depending on the RST line
#define EXTLIB_MFI_ERROR_INVALID_FLAGS          -2 //Invalid flags passed to function
#define EXTLIB_MFI_ERROR_OPENING_I2C            -3 //In the current implementation, this can never occur
#define EXTLIB_MFI_ERROR_INVALID_RESPONSE       -4 //Error communicating with MFi chip. 
#define EXTLIB_MFI_ERROR_INVALID_OUTBUF         -5 //User output buffer size is bad (not large enough for some operations, incompatible for others)
#define EXTLIB_MFI_ERROR_INVALID_INBUF          -6 //User input is bad (null pointer or input buffer to small)
#define EXTLIB_MFI_ERROR_UNKNOWN_OPERATION      -7 //User requested unsupported operation

/* MFi Operations */
#define MFI_GET_CERTIFICATE                     1
#define MFI_HASH_DATA                           2
#define MFI_READ_ERROR_REGISTER_VALUE           3

/* MFi_Open initializes I2C bus, then checks communication to the device by readings the device ID and comparing it against a known value for authentication chip version 2.0C */
signed long sl_ExtLib_MFi_Open (unsigned long address, unsigned char flags);
/* MFi Get command, performing operations on the MFi */
signed long sl_ExtLib_MFi_Get (unsigned char op, unsigned char* inbuf, 
                               signed long inbuf_len, unsigned char* outbuf, 
                               signed long outbuf_len);
/* MFI I2c close */
signed long sl_ExtLib_MFi_Close();


//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif
