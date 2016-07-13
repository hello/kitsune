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

#ifndef __WAC_USER_H__
#define __WAC_USER_H__

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

#include <stdint.h>
#include <rfc6234/sha.h>
#include <xdc/runtime/System.h>

#define SL_EXTLIB_AES_MAXNR                 14
#define SL_EXTLIB_AES_BLOCK_SIZE         	16
  
//*****************************************************************************
// if 'WAC_NW_STATUS_CALLBACK' defined  (in waclib project)
//			    then define this function in the application which will 
//              return the network status variable
// else 
//			No need of this function definition in the application
//*****************************************************************************
#ifdef  WAC_NW_STATUS_CALLBACK
#define sl_ExtLib_Get_WLAN_Status     SL_Get_WLAN_Status
#endif
#define sl_ExtLib_SHA_ctx_t         SHA1Context

// This should be a hidden type, but EVP requires that the size be known 
// Defined to make OpenSSL compatible
struct sl_aes_key_t {
#ifdef AES_LONG
unsigned long rd_key[4 *(SL_EXTLIB_AES_MAXNR + 1)];
#else
unsigned int rd_key[4 *(SL_EXTLIB_AES_MAXNR + 1)];
#endif
int rounds;
};
typedef struct sl_aes_key_t sl_ExtLib_aes_key_t;
   
//*****************************************************************************
// This function is workaround for now to get the network status inside library
//*****************************************************************************
#if defined(sl_ExtLib_Get_WLAN_Status)
extern unsigned long sl_ExtLib_Get_WLAN_Status();
#endif


//*****************************************************************************
//! External function for UART prints
//*****************************************************************************
#if defined(sl_ExtLib_UART_Print)
extern int sl_ExtLib_UART_Print(const char *pcFormat, ...)  ;
#endif


//*****************************************************************************
//
//! SHA Engine initialization 
//!
//! \param	SHA context
//!
//! \return 	success:0 else Error-code
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_SHA_Init)
extern int sl_ExtLib_SHA_Init(sl_ExtLib_SHA_ctx_t *C);
#endif

//*****************************************************************************
//
//! Update data to SHA engine to be processed
//!
//!	\param	SHA context
//!	\param	data to be processed
//!	\param	data length 
//!
//! \return 	success:0 else Error-code
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_SHA_Update)
extern int sl_ExtLib_SHA_Update(sl_ExtLib_SHA_ctx_t *c, const void *data, size_t len);
#endif

//*****************************************************************************
//
//! Get the SHA processed result
//!
//!	\param	final SHA processed data result
//!	\param	SHA context
//!
//! \return 	success:0 else Error-code
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_SHA_Final)
extern int sl_ExtLib_SHA_Final(unsigned char *md, sl_ExtLib_SHA_ctx_t *c);  
#endif  

//*****************************************************************************
//
//! To get the delay in b/w of execution
//!
//!	\param	time delay (mSec)
//!
//! \return 	None
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_Time_Delay)
extern void sl_ExtLib_Time_Delay(unsigned long ulDelay);
#endif
  
//*****************************************************************************
//
//! To set the AES encryption key
//!
//!	\param	user's encryption key
//!	\param 	length of key in bits
//!	\param	AES key context 
//!
//! \return 	success:0 else Error-code
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_AES_set_encrypt_key)
extern int sl_ExtLib_AES_set_encrypt_key(const unsigned char *userKey, const int bits,
                                        sl_ExtLib_aes_key_t *key);
#endif

//*****************************************************************************
//
//! To set the AES decryption key
//!
//!	\param	user's decryption key
//!	\param 	length of key in bits
//!	\param	AES key context 
//!
//! \return 	success:0 else Error-code
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_AES_set_decrypt_key)
extern int sl_ExtLib_AES_set_decrypt_key(const unsigned char *userKey, const int bits,
                                        sl_ExtLib_aes_key_t *key);
#endif

//*****************************************************************************
//
//! To encrypt the given data using CTR128 encryption mode
//!
//!	\param	input data to be encrypted
//!	\param	encrypted output data
//!	\param 	data length
//!	\param	AES key context 
//!	\param	initial vector
//!	\param	ecount buffer
//!	\param	num
//!
//! \return 	None
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_CTR128_Encrypt)
extern void sl_ExtLib_CTR128_Encrypt(const unsigned char *in, unsigned char *out,
                            size_t length, const sl_ExtLib_aes_key_t *key,
                            unsigned char ivec[SL_EXTLIB_AES_BLOCK_SIZE],
                            unsigned char ecount_buf[SL_EXTLIB_AES_BLOCK_SIZE],
                             unsigned int *num);
#endif


//*****************************************************************************
//
//! To decrypt the given data using CTR128 encryption mode
//!
//!	\param	input data to be encrypted
//!	\param	encrypted output data
//!	\param 	data length
//!	\param	AES key context 
//!	\param	initial vector
//!	\param	ecount buffer
//!	\param	num
//!
//! \return 	None
//!
//!	\note	User application needs to define this function in it's project
//
//*****************************************************************************
#if defined(sl_ExtLib_CTR128_Decrypt)
extern void sl_ExtLib_CTR128_Decrypt(const unsigned char *in, unsigned char *out,
                            size_t length, const sl_ExtLib_aes_key_t *key,
                            unsigned char ivec[SL_EXTLIB_AES_BLOCK_SIZE],
                            unsigned char ecount_buf[SL_EXTLIB_AES_BLOCK_SIZE],
                             unsigned int *num);
#endif

//*****************************************************************************
//
//! To generate key using CurveDonna algo
//!
//!	\param	output key
//! \param  input secret key
//! \param  input Basepoint
//!
//! \return 	None
//!
//
//*****************************************************************************
#if defined(sl_ExtLib_Curve_Donna)
void sl_ExtLib_Curve_Donna( unsigned char *outKey, const unsigned char *inSecret,
                            const unsigned char *inBasePoint );
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
