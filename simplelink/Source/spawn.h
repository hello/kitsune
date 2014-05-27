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

#ifndef __NONOS_H__
#define	__NONOS_H__

#ifdef	__cplusplus
extern "C" {
#endif


#if (defined (SL_PLATFORM_MULTI_THREADED)) && (!defined (SL_PLATFORM_EXTERNAL_SPAWN))

extern void _SlInternalSpawnTaskEntry();
extern int _SlInternalSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , unsigned long flags);

#undef sl_Spawn
#define sl_Spawn(pEntry,pValue,flags)               _SlInternalSpawn(pEntry,pValue,flags)

#undef _SlTaskEntry
#define _SlTaskEntry                                _SlInternalSpawnTaskEntry


#endif

#ifdef  __cplusplus
}
#endif // __cplusplus

#endif
