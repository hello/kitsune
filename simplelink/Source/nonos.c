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

#include "datatypes.h"
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"

#ifndef SL_PLATFORM_MULTI_THREADED

#include "nonos.h"

#define NONOS_MAX_SPAWN_ENTRIES		5

typedef struct
{
	_SlSpawnEntryFunc_t 		pEntry;
	void* 						pValue;
}_SlNonOsSpawnEntry_t;

typedef struct
{
	_SlNonOsSpawnEntry_t	SpawnEntries[NONOS_MAX_SPAWN_ENTRIES];
}_SlNonOsCB_t;

_SlNonOsCB_t g__SlNonOsCB;


_SlNonOsRetVal_t _SlNonOsSemSet(_SlNonOsSemObj_t* pSemObj , _SlNonOsSemObj_t Value)
{
	*pSemObj = Value;
	return NONOS_RET_OK;
}

_SlNonOsRetVal_t _SlNonOsSemGet(_SlNonOsSemObj_t* pSyncObj, _SlNonOsSemObj_t WaitValue, _SlNonOsSemObj_t SetValue, _SlNonOsTime_t Timeout)
{
	while (Timeout>0)
	{
		if (WaitValue == *pSyncObj)
		{
			*pSyncObj = SetValue;
			break;
		}
		if (Timeout != NONOS_WAIT_FOREVER)
		{		
			Timeout--;
		}
		_SlNonOsMainLoopTask();
	}
	
	if (0 == Timeout)
	{
		return NONOS_RET_ERR;
	}
	else
	{
		return NONOS_RET_OK;
	}
}


_SlNonOsRetVal_t _SlNonOsSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , unsigned long flags)
{
	int i;
	
	for (i=0 ; i<NONOS_MAX_SPAWN_ENTRIES ; i++)
	{
		_SlNonOsSpawnEntry_t* pE = &g__SlNonOsCB.SpawnEntries[i];
	
		if (NULL == pE->pEntry)
		{
			pE->pValue = pValue;
			pE->pEntry = pEntry;
			break;
		}
	}
        
        
        return NONOS_RET_OK;
}


_SlNonOsRetVal_t _SlNonOsMainLoopTask(void)
{
	int i;
	
	for (i=0 ; i<NONOS_MAX_SPAWN_ENTRIES ; i++)
	{
		_SlNonOsSpawnEntry_t* pE = &g__SlNonOsCB.SpawnEntries[i];
		_SlSpawnEntryFunc_t 		pF = pE->pEntry;
		
		if (NULL != pF)
		{
                        if((g_pCB)->RxIrqCnt != (g_pCB)->RxDoneCnt)
            {
			pF(0);//(pValue);
		}
			pE->pEntry = NULL;
			pE->pValue = NULL;
		}
	}
        
        return NONOS_RET_OK;
}
    
#endif //(SL_PLATFORM != SL_PLATFORM_NON_OS)
