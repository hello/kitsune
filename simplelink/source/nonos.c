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



/*****************************************************************************/
/* Include files                                                             */
/*****************************************************************************/
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"

#ifndef SL_PLATFORM_MULTI_THREADED

#include "nonos.h"


_SlNonOsCB_t g__SlNonOsCB = {0};



_SlNonOsRetVal_t _SlNonOsSemSet(_SlNonOsSemObj_t* pSemObj , _SlNonOsSemObj_t Value)
{
    *pSemObj = Value;
    return NONOS_RET_OK;
}

_SlNonOsRetVal_t _SlNonOsSemGet(_SlNonOsSemObj_t* pSyncObj, _SlNonOsSemObj_t WaitValue, _SlNonOsSemObj_t SetValue, _SlNonOsTime_t Timeout)
{
#if (!defined (SL_TINY)) && (defined(slcb_GetTimestamp))
      _SlTimeoutParams_t      TimeoutInfo={0};
#endif
      
	  /* If timeout 0 configured, just detect the value and return */
      if ((Timeout ==0) && (WaitValue == *pSyncObj))
	  {
		  *pSyncObj = SetValue;
		  return NONOS_RET_OK;
	  }

#if (!defined (SL_TINY)) && (defined(slcb_GetTimestamp))
      if ((Timeout != NONOS_WAIT_FOREVER) && (Timeout != NONOS_NO_WAIT))
      {
    	  _SlDrvStartMeasureTimeout(&TimeoutInfo, Timeout);
      }
#endif

#ifdef _SlSyncWaitLoopCallback
    _SlNonOsTime_t timeOutRequest = Timeout; 
#endif
    while (Timeout>0)
    {
        if (WaitValue == *pSyncObj)
        {
            *pSyncObj = SetValue;
            break;
        }
#if (!defined (slcb_GetTimestamp)) ||  (defined (SL_TINY))
        if (Timeout != NONOS_WAIT_FOREVER)
        {		
            Timeout--;
        }
#else        
        if ((Timeout != NONOS_WAIT_FOREVER) && (Timeout != NONOS_NO_WAIT))
        {
            if (_SlDrvIsTimeoutExpired(&TimeoutInfo))
            {
            	return (_SlNonOsRetVal_t)NONOS_RET_ERR;
            }

        }
 #endif

        /* If we are in cmd context and waiting for its cmd response
         * do not handle spawn async events as the global lock was already taken */
        if (FALSE == g_pCB->IsCmdRespWaited)
        {
        (void)_SlNonOsMainLoopTask();
        }
#ifdef _SlSyncWaitLoopCallback
        if( (__NON_OS_SYNC_OBJ_SIGNAL_VALUE == WaitValue) && (timeOutRequest != NONOS_NO_WAIT) )
        {
            if (WaitValue == *pSyncObj)
            {
                *pSyncObj = SetValue;
                break;
            }
            _SlSyncWaitLoopCallback();
        }
#endif
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


_SlNonOsRetVal_t _SlNonOsSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , _u32 flags)
{
	 _i8 i = 0;

     /* The paramater is currently not in use */
     (void)flags;
    
#ifndef SL_TINY 	
	for (i=0 ; i<NONOS_MAX_SPAWN_ENTRIES ; i++)
#endif     
	{
		_SlNonOsSpawnEntry_t* pE = &g__SlNonOsCB.SpawnEntries[i];
	
		if (pE->IsAllocated == FALSE)
		{
			pE->pValue = pValue;
			pE->pEntry = pEntry;
			pE->IsAllocated = TRUE;
#ifndef SL_TINY 	                        
			break;
#endif                        
		}
	}
        
        
    return NONOS_RET_OK;
}


_SlNonOsRetVal_t _SlNonOsMainLoopTask(void)
{
	_i8    i=0;
	void*  pValue;


#ifndef SL_TINY
	for (i=0 ; i<NONOS_MAX_SPAWN_ENTRIES ; i++)
#endif
	{
		_SlNonOsSpawnEntry_t* pE = &g__SlNonOsCB.SpawnEntries[i];
		

		if (pE->IsAllocated == TRUE)
		{
			_SlSpawnEntryFunc_t  pF = pE->pEntry;
			pValue = pE->pValue;


			/* Clear the entry */
			pE->pEntry = NULL;
			pE->pValue = NULL;
			pE->IsAllocated = FALSE;

			/* execute the spawn function */
            pF(pValue);
		}
	}
        
        return NONOS_RET_OK;
}

    
#endif /*(SL_PLATFORM != SL_PLATFORM_NON_OS)*/
