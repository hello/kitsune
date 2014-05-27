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

#ifndef __SL_DATA_TYPES__
#define __SL_DATA_TYPES__


#ifdef	__cplusplus
extern "C" {
#endif


#ifndef NULL
#define NULL        (0)
#endif

#ifndef FALSE
#define FALSE       (0)
#endif

#ifndef TRUE
#define TRUE        (!FALSE)
#endif

#ifndef OK
#define OK          (0)
#endif

#ifndef _INT8
#define _INT8
typedef signed   char   INT8;
#endif

#ifndef _UINT8
#define _UINT8
typedef unsigned char   UINT8;
#endif

#ifndef _INT16
#define _INT16
typedef signed   short  INT16;
#endif

#ifndef _UINT16
#define _UINT16
typedef unsigned short  UINT16;
#endif

#ifndef _BOOLEAN
#define _BOOLEAN
typedef unsigned char   BOOLEAN;
#endif

#ifdef _WIN32
    typedef unsigned int    UINT32, *PUINT32;
    typedef signed   int    INT32, *PINT32;
#else

#ifndef _INT32
#define _INT32
typedef signed   long   INT32;
#endif

#ifndef _UINT32
#define _UINT32
typedef unsigned long   UINT32;
#endif

#ifndef _UINT64
#define _UINT64
typedef unsigned long long   UINT64;
#endif

#endif /* _WIN32 */

typedef int             INT;
typedef char            CHAR;

typedef float			FLOAT;
typedef double			DOUBLE;

typedef UINT16  _SlOpcode_t;
typedef UINT8   _SlArgSize_t;
typedef INT16   _SlDataSize_t;
typedef INT     _SlReturnVal_t;

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* __SL_DATA_TYPE__ */
