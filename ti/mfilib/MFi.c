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

#include <string.h>

#include "MFi.h"
#include "mfilib.h"
#include "mfi_user.h"


sl_ExtLib_MFi_Handle_t MFi_Handle;


//****************************************************************************
//
//! This function is to read different requested data from MFI chip
//!
//! \param[in] opcode for different data from MFI
//! \param[in] input buffer to get hash from chip
//! \param[in] input buffer length
//! \param[out] output buffer
//! \param[in]  output buffer length
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long sl_ExtLib_MFi_Get (unsigned char op, unsigned char* inbuf, 
                               signed long inbuf_len, unsigned char* outbuf, 
                               signed long outbuf_len)
{
    signed long lRetVal=0;

    switch (op)
    {
        case (MFI_GET_CERTIFICATE):
        {
            if (outbuf_len < EXTLIB_MFI_MAX_CERTIFICATE_LENGTH)
                return EXTLIB_MFI_ERROR_INVALID_OUTBUF;
            else
            {
                lRetVal = _sl_ExtLib_MFi_GetCertificateData(outbuf); 
            }
            break;
        }
        case (MFI_HASH_DATA):
        {
            if (outbuf_len > EXTLIB_MFI_MAX_CHALLENGE_RESPONSE_LENGTH)
                return EXTLIB_MFI_ERROR_INVALID_OUTBUF;
            if ((inbuf == NULL) || \
                    (inbuf_len > EXTLIB_MFI_MAX_CHALLENGE_DATA_LENGTH))
                return EXTLIB_MFI_ERROR_INVALID_INBUF;
            else
            {
                lRetVal = _sl_ExtLib_MFi_GetChallengeResponse(inbuf, \
                                                            inbuf_len, outbuf); 
            }      
            break;
        }
        case (MFI_READ_ERROR_REGISTER_VALUE):
        {
            if (outbuf_len > sizeof(signed char))
                return EXTLIB_MFI_ERROR_INVALID_OUTBUF;
            else
            {
                lRetVal = _sl_ExtLib_MFi_ReadRegisterByte(ERROR_CODE_REG, \
                                                    (unsigned char*) outbuf);
            }      
            break;
        }    
        default:
            return EXTLIB_MFI_ERROR_UNKNOWN_OPERATION;    
          
    }

    return lRetVal;
}


//****************************************************************************
//
//! This function is to close MFI connection (i2c)
//!
//! \param None.
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long sl_ExtLib_MFi_Close()
{
    sl_ExtLib_I2C_Close(); 
    return EXTLIB_MFI_ERROR_OK;
}


//****************************************************************************
//
//! This function is to read different requested data from MFI chip
//!
//! \param[in] MFI chip address over which any data request will get raised
//! \param[in] I2C master mode
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long sl_ExtLib_MFi_Open (unsigned long MFi_Address, unsigned char flags)
{
    // Check for illegitemate I2C address 
    if (MFi_Address == NULL) 
    {
        return EXTLIB_MFI_ERROR_INVALID_ADDRESS;
    }
    else
    {
        //Update our I2C address
        MFi_Handle.MFiAddress = MFi_Address; 
    }
  
    // Check for illegitemate flags 
    if ((flags != MFI_I2C_MASTER_MODE_STD) && (flags != MFI_I2C_MASTER_MODE_FST))
    {
        return EXTLIB_MFI_ERROR_INVALID_FLAGS;
    }
    else 
    {
        return _sl_ExtLib_MFi_Init(flags);
    }
    //return EXTLIB_MFI_ERROR_OK;
}


//****************************************************************************
//
//! This function is initialize MFI interface and authenticate MFI chip
//!
//! \param[in] I2C mode
//!
//! \return success:0, else:error code
//
//! \warning  Application must has to do pinmux for i2c before this API call
//
//****************************************************************************
signed long int _sl_ExtLib_MFi_Init(unsigned char flags)
{  
    signed long lError=0;
  
    //Init I2C Module
    lError = sl_ExtLib_I2C_Open(flags); 
    if (lError < 0)
    {
        return EXTLIB_MFI_ERROR_OPENING_I2C;
    }
  
    //Dummy read to check if the MFi device is attached and operational
    if (!(_sl_ExtLib_MFi_VerifyDeviceExists())) 
    {
        MFi_Handle.MFiExists = 1;
        return EXTLIB_MFI_ERROR_OK;
    }
    else 
    {
        MFi_Handle.MFiExists = 0;
        return EXTLIB_MFI_ERROR_INVALID_RESPONSE;
    }
}

//****************************************************************************
//
//! This function is get certificate data from MFI chip
//!
//! \param[out] output buffer that contains certificate data
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long _sl_ExtLib_MFi_GetCertificateData(unsigned char* outbuf)
{
    unsigned short usAuxShort = 0;
    unsigned short usCertDataLength = 0;
    signed long lStatus = -1;
    unsigned short usTempCount = 0;
    
    lStatus = _sl_ExtLib_MFi_SetRegister (ACCESSORY_CERT_DATA_LEN);
    if(lStatus < 0)
    {
      return lStatus;
    }
    
    lStatus = -1;
    while (lStatus < 0)
    {  
        // Read from selected register 2 byte 
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress, \
                                        (unsigned char*) &usCertDataLength, 2);  
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }
  
    // Store certificate data length
    usCertDataLength = SL_MFI_SWAP16 (usCertDataLength); 
  
    lStatus = _sl_ExtLib_MFi_SetRegister (ACCESSORY_CERT_DATA_1);
    if(lStatus < 0)
    {
      return lStatus;
    }
    
    usAuxShort=0; 
    while ((usAuxShort + 128) < usCertDataLength)
    {
        // Read 128 bytes at a time
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress, \
                                    (unsigned char*) outbuf + usAuxShort, 0x80);
    
        if (lStatus == 0) 
        {
            usAuxShort += 0x80;
            usTempCount = 0;
        }
    
        if (usTempCount++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }
  
    lStatus = -1;
    usTempCount = 0;
  
    /* Take care of residue */
    while (lStatus < 0)
    {
        // Read 128 bytes at a time
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress, \
                                        (unsigned char*) outbuf + usAuxShort, \
                                        usCertDataLength - usAuxShort);    
        if (usTempCount++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }
  
    return usCertDataLength;
}


//****************************************************************************
//
//! This function is read register bytes from MFI chip
//!
//! \param[in]  register address
//! \param[out] register data read from given address
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long _sl_ExtLib_MFi_ReadRegisterByte (unsigned char registerAddress,     
                                        unsigned char* registerData)
{
    unsigned char auxArray[2] = {0};
    unsigned short usAuxShort = 0;
    signed long lStatus = -1;

    auxArray[0] = registerAddress;

    while (lStatus < 0)
    {
        // Write to MFi, 1 bytes (address), and no stop bit 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray[0], 1, 0);  
        
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }
    
    usAuxShort = 0;
    lStatus = -1;

    while (lStatus < 0)
    {  
        // Read from selected register 1 byte 
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress, registerData, 1);
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }
  
    return EXTLIB_MFI_ERROR_OK;
}


//****************************************************************************
//
//! This function is write address to register to read any data
//!
//! \param[in]  register address
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long _sl_ExtLib_MFi_SetRegister (unsigned char registerAddress)
{
    unsigned char auxArray = 0;
    unsigned short usAuxShort = 0;
    signed long lStatus = -1;

    auxArray = registerAddress;

    while (lStatus < 0)
    {
        // Write to MFi, 2 bytes (address and 1 byte of data), no stop bits 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray, 1, 0);  
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }  
  
    return lStatus;   
}

//****************************************************************************
//
//! This function is write data to given address
//!
//! \param[in]  register address
//!
//! \param[in]  register data
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long _sl_ExtLib_MFi_WriteRegisterByte (unsigned char registerAddress, 
                                                unsigned char registerData)
{
    unsigned char auxArray[2] = {0};
    unsigned short usAuxShort = 0;
    signed long lStatus = 0;

    auxArray[0] = registerAddress;
    auxArray[1] = registerData;

    while (lStatus < 0)
    {
        // Write to MFi, 2 bytes (address and 1 byte of data),
        // 1 stop bit to indicate end of command 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray[0], 2, 1);  
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }  
  
    return lStatus;
}

//****************************************************************************
//
//! This function is verify if MFI device is exist and connected to MCU
//!
//! \param[in]  None
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long _sl_ExtLib_MFi_VerifyDeviceExists()
{
    signed long lStatus = -1;
    unsigned char data = 0;

    lStatus = _sl_ExtLib_MFi_ReadRegisterByte (DEVICE_VERSION_REG, &data);  

    if ((lStatus == 0) && (data == DEVICE_REVISION)) 
        return 0;
    else 
        return -1;
}

//****************************************************************************
//
//! This function is get key/hash from MFI chip
//!
//! \param[in]  challenge data to feed into MFI
//!
//! \param[in]  challenge data length
//!
//! \param[out] output data buffer
//!
//! \return success:0, else:error code
//
//****************************************************************************
signed long _sl_ExtLib_MFi_GetChallengeResponse (unsigned char* challengeData, 
                                            unsigned short challengeDataLength, 
                                            unsigned char* responseData)
{
    unsigned char auxArray[32]={0};
    unsigned char auxChar=0;
    unsigned short usAuxShort=0;
    unsigned short challengeRespLength=0;
    signed long lStatus=-1;
    
    /* Write challenge length & data*/
    auxArray[0] = CHALLENGE_DATA_LEN;
    // Two bytes for challenge length, but only values up to 128 bytes are valid, 
    // so upper byte is meaningless
    auxArray[1] = 0; 
    auxArray[2] = challengeDataLength;
    memcpy (&auxArray[3], challengeData, challengeDataLength);
  
    while (lStatus < 0)
    {
        // Write to MFi, 1 bytes (address), 1 byte length, and stop bit 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray[0], \
                                        3 + challengeDataLength, 1);    
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }  

    lStatus = -1;
    usAuxShort = 0;

    /* Trigger new challange response */
    auxArray[0] = CONTROL_AND_STATUS;
    auxArray[1] = START_NEW_CHALLENGE_RESPONSE;

    while (lStatus < 0)
    {
        // Write to MFi, 1 bytes (address), and stop bit 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray[0], 2, 1);    
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }  

    lStatus = -1;
    usAuxShort = 0;
  
    //Delay for 50m - give the MFi Chip some time to process
    sl_ExtLib_Time_Delay(50); 
  
    /* Read lStatus */
    auxArray[0] = CONTROL_AND_STATUS;
    
    while (lStatus < 0)
    {
        // Write to MFi, 1 bytes (address), and no stop bit 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray[0], 1, 0);    
        if (usAuxShort++ > 60000) 
        {
            lStatus = -1;
            return lStatus;
        }
    }

    lStatus = -1;
    usAuxShort = 0;
  
    // Poll until challenge data is ready 
    while (auxChar != CHALLENGE_RESP_GENERATED)
    {  
        // Read from selected register 1 byte 
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress,\
                                        (unsigned char*) &auxChar, 1);  
    
        if (usAuxShort++ > 60000) 
        {
            lStatus = -1;
            return lStatus;
        }    
    }
  
    lStatus = -1;
    usAuxShort = 0;
  
    // Read challenge response data length 
    auxArray[0] = CHALLENGE_RESP_LEN;
  
    while (lStatus < 0)
    {
        // Write to MFi, 1 bytes (address), and no stop bit 
        lStatus = sl_ExtLib_I2C_Write(MFi_Handle.MFiAddress, &auxArray[0], 1, 0);    
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }

    lStatus = -1;
    usAuxShort = 0;
  
    while (lStatus < 0)
    {  
        // Read from selected register 2 byte 
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress, \
                                    (unsigned char*) &challengeRespLength, 2);  
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }
  
    //Store challenge response length
    challengeRespLength = SL_MFI_SWAP16 (challengeRespLength); 

    lStatus = -1;
    usAuxShort = 0;  
  
    // Read challenge response data 
    usAuxShort = 0;
    while (lStatus < 0)
    {
        lStatus = sl_ExtLib_I2C_Read(MFi_Handle.MFiAddress, \
                            (unsigned char*) responseData, challengeRespLength); 
        if (usAuxShort++ > MFI_TIMEOUT) 
        {
            lStatus = -1;
            return lStatus;
        }
    }  
  
    return challengeRespLength;
}
