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

#ifndef __NETUTIL_H__
#define __NETUTIL_H__

/*****************************************************************************/
/* Include files                                                             */
/*****************************************************************************/
#include "simplelink.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
	\defgroup NetUtil 
	\short Networking related commands and configuration

*/

/*!

    \addtogroup NetUtil
    @{

*/

/*****************************************************************************/
/* Macro declarations                                                        */
/*****************************************************************************/

/* Set/get options */
#define SL_NETUTIL_CRYPTO_PUBLIC_KEY 				(1)
#define SL_NETUTIL_CRYPTO_PUBLIC_KEY_INFO			(2)


/* Commands */
#define SL_NETUTIL_CRYPTO_CMD_CREATE_CERT		 	(1)
#define SL_NETUTIL_CRYPTO_CMD_SIGN_MSG 			(2)
#define SL_NETUTIL_CRYPTO_CMD_VERIFY_MSG		    (3)
#define SL_NETUTIL_CRYPTO_CMD_TEMP_KEYS			(4)
#define SL_NETUTIL_CRYPTO_CMD_INSTALL_OP          (5)
        

/*****************************************************************************/
/* Errors returned from the general error async event                        */
/*****************************************************************************/


/*****************************************************************************/
/* Structure/Enum declarations                                               */
/*****************************************************************************/

/* Defines the size of the buffer that will be alocated */
/* (on the stack) by the sl_UtilsCmd API.               */
#define SL_NETUTIL_CMD_BUFFER_SIZE						(256)

/* Enumaration of Signature types */
#define SL_NETUTIL_CRYPTO_SIG_SHAwDSA                     (0)
#define SL_NETUTIL_CRYPTO_SIG_MD2wRSA                     (1)
#define SL_NETUTIL_CRYPTO_SIG_MD5wRSA                     (2)
#define SL_NETUTIL_CRYPTO_SIG_SHAwRSA                     (3)
#define SL_NETUTIL_CRYPTO_SIG_SHAwECDSA                   (4)
#define SL_NETUTIL_CRYPTO_SIG_SHA256wRSA                  (5)
#define SL_NETUTIL_CRYPTO_SIG_SHA256wECDSA                (6)
#define SL_NETUTIL_CRYPTO_SIG_SHA384wRSA                  (7)
#define SL_NETUTIL_CRYPTO_SIG_SHA384wECDSA                (8)
#define SL_NETUTIL_CRYPTO_SIG_SHA512wRSA                  (9)
#define SL_NETUTIL_CRYPTO_SIG_SHA512wECDSA                (10)
/* Add more signature-Types here */

/* Digest length definitions */
#define SL_NETUTIL_CRYPTO_DGST_MD2_LEN_BYTES              (16)
#define SL_NETUTIL_CRYPTO_DGST_MD5_LEN_BYTES              (16)
#define SL_NETUTIL_CRYPTO_DGST_SHA_LEN_BYTES              (20)
#define SL_NETUTIL_CRYPTO_DGST_SHA256_LEN_BYTES           (32)
#define SL_NETUTIL_CRYPTO_DGST_SHA384_LEN_BYTES           (48)
#define SL_NETUTIL_CRYPTO_DGST_SHA512_LEN_BYTES           (64)


/* Enumeration of Create-Certificate sub-commands */
#define SL_NETUTIL_CRYPTO_CERT_INIT                       (1)
#define SL_NETUTIL_CRYPTO_CERT_SIGN_AND_SAVE              (2)
#define SL_NETUTIL_CRYPTO_CERT_VER                        (3)
#define SL_NETUTIL_CRYPTO_CERT_SERIAL                     (4)
#define SL_NETUTIL_CRYPTO_CERT_SIG_TYPE                   (5)
#if 0 /* reserved for Issuer information - currently not supported */
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_COUNTRY             (6)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_STATE               (7)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_LOCALITY            (8)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_SUR                 (9)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_ORG                 (10)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_ORG_UNIT            (11)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_COMMON_NAME         (12)
#define SL_NETUTIL_CRYPTO_CERT_ISSUER_EMAIL               (13)
#endif /* End - issuer information */
#define SL_NETUTIL_CRYPTO_CERT_DAYS_VALID                 (14)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_COUNTRY            (15)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_STATE              (16)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_LOCALITY           (17)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_SUR                (18)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_ORG                (19)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_ORG_UNIT           (20)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_COMMON_NAME        (21)
#define SL_NETUTIL_CRYPTO_CERT_SUBJECT_EMAIL              (22)
#define SL_NETUTIL_CRYPTO_CERT_IS_CA                      (23)


/* Enumeration of "Temp-Keys" commands */
#define SL_NETUTIL_CRYPTO_TEMP_KEYS_CREATE                (1)
#define SL_NETUTIL_CRYPTO_TEMP_KEYS_REMOVE                (2)

/* Enumeration of "Install/Uninstall" sub-commands */
#define SL_NETUTIL_CRYPTO_INSTALL_SUB_CMD                 (1)
#define SL_NETUTIL_CRYPTO_UNINSTALL_SUB_CMD               (2)


/* The reserved key for IOT Usage */
#define SL_NETUTIL_CRYPTO_SERVICES_IOT_RESERVED_INDEX	   (0)

/* The Temporary key for FS Usage */
#define SL_NETUTIL_CRYPTO_FS_TEMP_KEYS_OBJ_ID				(1)


/**********************************************/
/* Public Key Info Structures and Definitions */
/**********************************************/

/* Enumeration of Elliptic Curve "named" curves */
#define SL_NETUTIL_CRYPTO_EC_NAMED_CURVE_NONE            (0)
#define SL_NETUTIL_CRYPTO_EC_NAMED_CURVE_SECP256R1       (1)

/* PLACE HOLDER for future definitions of custom-curve parameters */
typedef struct
{
    _u8 					Padding[4];
} SlNetUtilCryptoEcCustomCurveParam_t;


/* Union holding the Elliptic Curve parameters. */
typedef union
{
    _u8	                        			NamedCurveParams;  /* parameters for named-curve (the curve identifier) */
    SlNetUtilCryptoEcCustomCurveParam_t		CustomCurveParams; /* parameters for custom curves */
} SlNetUtilCryptoEcCurveParams_u;


/* “curve-type” definitions */
#define SL_NETUTIL_CRYPTO_EC_CURVE_TYPE_NAMED			(1)  /* ECC Named Curve type */
#define SL_NETUTIL_CRYPTO_EC_CURVE_TYPE_CUSTOM			(2)  /* ECC Custom curve type */


/* Enumeration of the supported public-key algorithms */
#define SL_NETUTIL_CRYPTO_PUB_KEY_ALGO_NONE           (0)
#define SL_NETUTIL_CRYPTO_PUB_KEY_ALGO_EC             (1)


/* Structure for holding the Elliptic Curve Key parameters */
typedef struct
{	
    _u8									CurveType;			/* defines curve type - custom or named */
    SlNetUtilCryptoEcCurveParams_u		CurveParams;		/* specific parameters of the curve (depends on curve_type) */
} SlNetUtilCryptoEcKeyParams_t;

/* Union for holding the Public Key parameters, depends on key algorithm */
typedef union
{

    SlNetUtilCryptoEcKeyParams_t 		EcParams;			/* parameters for Elliptic Curve key */

    /* add containers for other key types and algos here*/

} SlNetUtilCryptoPubKeyParams_u;



/* structure for holding all the meta-data about a key-pair  */
typedef struct
{
    _u8		                   	  				KeyAlgo;
    SlNetUtilCryptoPubKeyParams_u  				KeyParams;
	_u8		                                    KeyFileNameLen;
    _u8	                                        CertFileNameLen;
}SlNetUtilCryptoPubKeyInfo_t;

/********************************************/
/* NetUtil-Crypto Cmd "Attributes" structures */
/********************************************/
/* structure for holding all the attributes for a "Sign" Command */
typedef struct
{
    _u32       ObjId;
    _u32       SigType;
    _u32       Flags;
} SlNetUtilCryptoCmdSignAttrib_t;


/* structure for holding all the attributes for a "Verify" Command */
typedef struct
{
    _u32       ObjId;
    _u32       SigType;
    _u32       Flags;
    _u16       MsgLen;
    _u16       SigLen;
} SlNetUtilCryptoCmdVerifyAttrib_t;

/* structure for holding all the attributes for a "Create Certificate" Command */
typedef struct
{
    _u32       ObjId;
    _u32       Flags;
    _u16       SubCmd;
} SlNetUtilCryptoCmdCreateCertAttrib_t;

/* structure for holding all the attributes for  "Key management" Commands: */
/* Temp-Key (create and delete), Install and un-Install.                    */
typedef struct
{
    _u32       ObjId;
    _u32       Flags;
    _u16       SubCmd;
} SlNetUtilCryptoCmdKeyMgnt_t;



/******************************************************************************/
/* Type declarations                                                          */
/******************************************************************************/

/*****************************************************************************/
/* Function prototypes                                                       */
/*****************************************************************************/

/*!
    \brief     Function for setting configurations of utilities  
   
    \param[in] Option		Identifier of the specific "set" operation to perform 
    \param[in] ObjID	    ID of the relevant object that this set operation will be performed on
    \param[in] ValueLen		Length of the value parameter
    \param[in] pValues		Pointer to the buffer holding the configurations values
    \return    Zero on success, or -1 on failure
    \sa       sl_NetUtilGet  sl_NetUtilCmd
    \note 
    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_NetUtilSet)
_i32 sl_NetUtilSet(const _u16 Option, const _u32 ObjID, const _u8 *pValues,  const _u16 ValueLen);
#endif

/*!
    \brief     Function for getting configurations of utilities  

    \param[in]		Option		Identifier of the specific "get" operation to perform 
    \param[in] 		ObjID	  	ID of the relevant object that this set operation will be performed on
    \param[in,out] 	pValueLen	Pointer to the length of the value parameter\n
								On input - provides the length of the buffer that the application allocates, and
								will hold the output\n
								On output - provides the actual length of the received data
    \param[out] 	pValues		Pointer to the buffer that the application allocates, and will hold
                                the received data
    \return    Zero on success, or -1 on failure
    \sa        sl_NetUtilSet sl_NetUtilCmd
    \note 
    \warning
*/
#if _SL_INCLUDE_FUNC(sl_NetUtilGet)
_i16 sl_NetUtilGet(const _u16 Option, const _u32 ObjID, _u8 *pValues, _u16 *pValueLen);
#endif

/*!
    \brief     Function for performing utilities-related commands  

    \return    On success, zero is returned. On error, -1 is returned
   
    \param[in]		Cmd 			Identifier of the specific Command to perform 
    \param[in]		pAttrib	    	Pointer to the buffer holding the Attribute values
    \param[in]		AttribLen		Length of the Attribute-values 
    \param[in] 		pInputValues	Pointer to the buffer holding the input-value
    \param[in] 		InputLen 		Length of the input-value
    \param[out] 	pOutputValues	Pointer to the buffer that the application allocates, and will hold
                                    the received data
    \param[in,out]  pOutputLen 		Length of the output-value \n
									On input - provides the length of the buffer that the application allocates, and
                                    will hold the output\n
 									On output - provides the actual length of the received output-values
    \return    Zero on success, or -1 on failure
    \sa       sl_NetUtilGet sl_NetUtilSet  
    \note
    \warning
*/
#if _SL_INCLUDE_FUNC(sl_NetUtilCmd)
_i16 sl_NetUtilCmd(const _u16 Cmd,  const _u8 *pAttrib, const _u16 AttribLen,
				 const _u8 *pInputValues, const _u16 InputLen,
				 _u8 *pOutputValues,_u16 *pOutputLen );
#endif

/*!

 Close the Doxygen group.
 @}

 */


#ifdef  __cplusplus
}
#endif /*  __cplusplus */

#endif  /*  __NETUTIL_H__ */


