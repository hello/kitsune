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

#ifndef __MFI_H__
#define __MFI_H__


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
  

#define MFI_TIMEOUT          0xFF
#define DEVICE_REVISION      0x05

/* MFi Registers: */
#define DEVICE_VERSION_REG      0x0       
#define FIRMWARE_VERSION        0x1
#define AUTH_PROT_MAJOR_VER     0x2
#define AUTH_PROT_MINOR_VER     0x3
#define DEVICE_ID               0x4
#define ERROR_CODE_REG          0x5
#define CONTROL_AND_STATUS      0x10
#define CHALLENGE_RESP_LEN      0x11
#define CHALLENGE_RESP_DATA     0x12
#define CHALLENGE_DATA_LEN      0x20
#define CHALLENGE_DATA          0x21
#define ACCESSORY_CERT_DATA_LEN 0x30
#define ACCESSORY_CERT_DATA_1   0x31
#define ACCESSORY_CERT_DATA_2   0x32
#define ACCESSORY_CERT_DATA_3   0x33
#define ACCESSORY_CERT_DATA_4   0x34
#define ACCESSORY_CERT_DATA_5   0x35
#define ACCESSORY_CERT_DATA_6   0x36
#define ACCESSORY_CERT_DATA_7   0x37
#define ACCESSORY_CERT_DATA_8   0x38
#define ACCESSORY_CERT_DATA_9   0x39
#define ACCESSORY_CERT_DATA_10  0x3A
#define SELF_TEST_CONT_STATUS   0x40
#define SYSTEM_EVENT_COUNTER    0x4D
#define DEVICE_CERT_DATA_LEN    0x50
#define DEVICE_CERT_DATA_1      0x51
#define DEVICE_CERT_DATA_2      0x52
#define DEVICE_CERT_DATA_3      0x53
#define DEVICE_CERT_DATA_4      0x54
#define DEVICE_CERT_DATA_5      0x55
#define DEVICE_CERT_DATA_6      0x56
#define DEVICE_CERT_DATA_7      0x57
#define DEVICE_CERT_DATA_8      0x58

/* MFi Constants */
#define EXTLIB_MFI_MAX_CERTIFICATE_LENGTH       1280
#define EXTLIB_MFI_MAX_CHALLENGE_DATA_LENGTH    0xFFFF
#define EXTLIB_MFI_MAX_CHALLENGE_RESPONSE_LENGTH        EXTLIB_MFI_MAX_CHALLENGE_DATA_LENGTH

enum _sl_ExtLib_MFi_procResults
{
    INVALID_RESULT = 0, //0
    CHALLENGE_RESP_GENERATED = 0x10, //1
    CHALLENGE_GRANTED = 0x20, 
    DEVICE_VERIFIED = 0x30, 
    DEVICE_CERTIFICATE_VALIDATED = 0x40 //4
};

enum _sl_ExtLib_MFi_ProcControl
{
    NO_OPERATION, //0
    START_NEW_CHALLENGE_RESPONSE, //1
    START_NEW_CHALLENGE_GENERATION, 
    START_NEW_CHALLENGE_VERIFICATION, 
    START_NEW_CERTIFICATE_VALIDATION //4
};

enum _sl_ExtLib_MFi_ErrorCodes
{
    ERROR_OK, //0
    ERROR_INVALID_READ_REGISTER, //1
    ERROR_INVALID_WRITE_REGISTER, 
    ERROR_INVALID_CHALLENGE_RESPONSE_LENGTH, 
    ERROR_INVALID_CHALLENGE_LENGTH, //4
    ERROR_INVALID_CERTIFICATE_LENGTH,
    ERROR_INTERNAL_CHALLENGE_RESPONSE_ERROR,
    ERROR_INTERNAL_CHALLENGE_ERROR,
    ERROR_INTERNAL_CHALLENGE_VERIFICATION_ERROR,
    ERROR_INTERNAL_CERTIFICATE_VALIDATION_ERROR,
    ERROR_INVALID_PROCESS_CONTROL, 
    ERROR_OUT_OF_SEQUENCE //0xB
};

typedef struct{
    volatile unsigned char  MFiExists;
    volatile unsigned char  MFiAddress;
} sl_ExtLib_MFi_Handle_t;

signed long _sl_ExtLib_MFi_GetCertificateData (unsigned char* inbuf);
signed long _sl_ExtLib_MFi_ReadRegisterByte (unsigned char registerAddress, unsigned char* registerData);
signed long _sl_ExtLib_MFi_WriteRegisterByte (unsigned char registerAddress, unsigned char registerData);
signed long _sl_ExtLib_MFi_SetRegister (unsigned char registerAddress);
signed long _sl_ExtLib_MFi_VerifyDeviceExists();
signed long _sl_ExtLib_MFi_GetChallengeResponse (unsigned char* challengeData, unsigned short challengeDataLength,\
                                                unsigned char* responseData);
signed long _sl_ExtLib_MFi_Init(unsigned char flags);



//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif
