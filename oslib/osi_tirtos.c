//*****************************************************************************
// osi_tirtos.c
//
// Interface APIs for TI-RTOS function calls
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* XDCtools Header files */
#include <xdc/std.h>
//#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/HeapStd.h>

#include <ti/sysbios/BIOS.h>

#include <ti/sysbios/family/arm/m3/Hwi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Mailbox.h>

#include <osi.h>

//Local function definition
static void vSimpleLinkSpawnTask( void *pvParameters );

Mailbox_Handle simpleLink_mailbox;
HeapStd_Handle g_GlobalHeapHdl;
// Queue size
#define slQUEUE_SIZE				( 3 )

#define TICK_RATE_MS            (1000/1000)

/*!
	\brief 	This function registers an interrupt in NVIC table

	The sync object is used for synchronization between different thread or ISR and
	a thread.

	\param	iIntrNum	-	Interrupt number to register
	\param	pEntry	    -	Pointer to the interrupt handler
	\param ucPriority   -   Priority of the interrupt

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_InterruptRegister(int iIntrNum,P_OSI_INTR_ENTRY pEntry,unsigned char ucPriority)
{

	Hwi_Handle hHandle =  Hwi_getHandle(iIntrNum);// Returns Hwi_handle associated with intNum(UInt intNum);
	if(hHandle)
	{
		Hwi_delete(&hHandle);
		hHandle = 0;
	}
	Hwi_FuncPtr hwiFxn = (Hwi_FuncPtr)pEntry;
	hHandle = Hwi_create(iIntrNum, hwiFxn, NULL, NULL);
	Hwi_setPriority(iIntrNum, ucPriority);
	return (int)hHandle;
}

/*!
	\brief 	This function De registers an interrupt in NVIC table

	\param	iIntrNum	-	Interrupt number to register

	\return none
	\note
	\warning
*/
void osi_InterruptDeRegister(int iIntrHdl)
{
	Hwi_delete((ti_sysbios_family_arm_m3_Hwi_Handle*)&iIntrHdl);
}


/*!
	\brief 	This function creates a sync object

	The sync object is used for synchronization between different thread or ISR and
	a thread.

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjCreate(OsiSyncObj_t* pSyncObj)
{
    Semaphore_Handle sem_handle = NULL;
	Error_Block eb;

	Error_init(&eb);

	sem_handle = Semaphore_create(1, NULL, &eb);
	if (NULL == sem_handle) {
		return OSI_FAILURE;
	}

	*(Semaphore_Handle *)pSyncObj = sem_handle;
    return OSI_OK;
}

/*!
	\brief 	This function deletes a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjDelete(OsiSyncObj_t* pSyncObj)
{
    Semaphore_delete((Semaphore_Handle *)*pSyncObj);
     return OSI_OK;   /* Semaphore_delete doesn't return the status */
}

/*!
	\brief 		This function generates a sync signal for the object.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signalling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function could called from ISR context
	\warning
*/
int osi_SyncObjSignal(OsiSyncObj_t* pSyncObj)
{
    Semaphore_post((ti_sysbios_knl_Semaphore_Handle)*pSyncObj);
    return OSI_OK; /* Semaphore_post doesn't return the status */
}

/*!
	\brief 		This function generates a sync signal for the object.
				from ISR context.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signalling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function is be called from ISR context
	\warning
*/
int osi_SyncObjSignalFromISR(OsiSyncObj_t* pSyncObj)
{
	Semaphore_post((ti_sysbios_knl_Semaphore_Handle)*pSyncObj);
    return OSI_OK; /* Semaphore_post doesn't return the status */
}

/*!
	\brief 	This function waits for a sync signal of the specific sync object

	\param	pSyncObj	-	pointer to the sync object control block
	\param	Timeout		-	numeric value specifies the maximum number of mSec to
							stay suspended while waiting for the sync signal
							Currently, the simple link driver uses only two values:
								- OSI_WAIT_FOREVER
								- OSI_NO_WAIT

	\return upon successful reception of the signal within the timeout window return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjWait(OsiSyncObj_t* pSyncObj , OsiTime_t Timeout)
{
   signed int ret_status = FALSE;
   	unsigned int time_out = 0;

	switch(Timeout)
	{
		case OSI_NO_WAIT: time_out = BIOS_NO_WAIT; break;
		case OSI_WAIT_FOREVER: time_out = BIOS_WAIT_FOREVER; break;
		default: time_out = Timeout/TICK_RATE_MS; break;
	}

	/* TBD: Check TRUE or FALSE vs return value of Semaphore_pend */
	ret_status = Semaphore_pend((ti_sysbios_knl_Semaphore_Handle)*pSyncObj, time_out);
	if (TRUE != ret_status){
		return OSI_FAILURE;
	}

    return OSI_OK;
}

/*!
	\brief 	This function clears a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful clearing the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjClear(OsiSyncObj_t* pSyncObj)
{
	//ti_sysbios_knl_Semaphore_Object *pSemObj;
	//pSemObj =(ti_sysbios_knl_Semaphore_Object *)(*pSyncObj);
	if (osi_SyncObjWait(pSyncObj,0) == OSI_OK )
    {
        return OSI_OK;
    }
    else
    {
        return OSI_FAILURE;
    }
}

/*!
	\brief 	This function creates a locking object.

	The locking object is used for protecting a shared resources between different
	threads.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_LockObjCreate(OsiLockObj_t* pLockObj)
{
    Semaphore_Handle sem_handle = NULL;
    Semaphore_Params params;
	Error_Block eb;

	Error_init(&eb);
	Semaphore_Params_init(&params);
	params.mode = Semaphore_Mode_BINARY;

	sem_handle = Semaphore_create(1, &params, &eb);
	if (NULL == sem_handle) {
		return OSI_FAILURE;
	}

	*(Semaphore_Handle *)pLockObj = sem_handle;
    return OSI_OK;
}

/*!
	\brief 	This function creates a Task.

	Creates a new Task and add it to the last of tasks that are ready to run

	\param	pEntry	-	pointer to the Task Function
	\param	pcName	-	Task Name String
	\param	usStackDepth	-	Stack Size in bytes
	\param	pvParameters	-	pointer to structure to be passed to the Task Function
	\param	uxPriority	-	Task Priority

	\return upon successful creation the function should return 1
			Otherwise, 0 or a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_TaskCreate(P_OSI_TASK_ENTRY pEntry,const signed char * const pcName,unsigned short usStackDepth,
void *pvParameters,unsigned long uxPriority,OsiTaskHandle pTaskHandle)
{
   Task_Handle tsk_handle = NULL;
       Task_Params taskParams;
       Error_Block eb;

       Error_init(&eb);

       Task_Params_init(&taskParams);
       taskParams.stackSize = usStackDepth; //Stack size needs to be given in Bytes for TI-RTOS
       taskParams.priority = uxPriority;
       taskParams.arg0 = (xdc_UArg)pvParameters;
       taskParams.arg1 = 0;
       //taskParams.vitalTaskFlag = false;

       tsk_handle = Task_create((ti_sysbios_knl_Task_FuncPtr)pEntry, &taskParams, &eb);
       pTaskHandle = (OsiTaskHandle *)tsk_handle;
       if (NULL == pTaskHandle) {
           return OSI_FAILURE;
       }


    return OSI_OK;

}


/*!
	\brief 	This function Deletes a Task.

	Deletes a  Task and remove it from list of running task

	\param	pTaskHandle	-	Task Handle

	\note
	\warning
*/
void osi_TaskDelete(OsiTaskHandle pTaskHandle)
{
	Task_delete((Task_Handle *)pTaskHandle);

}



/*!
	\brief 	This function deletes a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_LockObjDelete(OsiLockObj_t* pLockObj)
{
     Semaphore_delete((Semaphore_Handle *)*pLockObj);
     return OSI_OK;   /* Semaphore_delete doesn't return the status */
}

/*!
	\brief 	This function locks a locking object.

	All other threads that call this function before this thread calls
	the osi_LockObjUnlock would be suspended

	\param	pLockObj	-	pointer to the locking object control block
	\param	Timeout		-	numeric value specifies the maximum number of mSec to
							stay suspended while waiting for the locking object
							Currently, the simple link driver uses only two values:
								- OSI_WAIT_FOREVER
								- OSI_NO_WAIT


	\return upon successful reception of the locking object the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_LockObjLock(OsiLockObj_t* pLockObj , OsiTime_t Timeout)
{
    signed int ret_status = FALSE;
	unsigned int time_out = 0;

	    switch(Timeout)
	    {
	        case OSI_NO_WAIT: time_out = BIOS_NO_WAIT; break;
	        case OSI_WAIT_FOREVER: time_out = BIOS_WAIT_FOREVER; break;//BIOS_WAIT_FOREVER
	        default: time_out = Timeout/TICK_RATE_MS; break;
	    }

	    /* TBD: Check TRUE or FALSE vs return value of Semaphore_pend */
	    ret_status = Semaphore_pend((ti_sysbios_knl_Semaphore_Handle)*pLockObj, time_out);
	    if (TRUE != ret_status){
	        return OSI_FAILURE;
	    }

    return OSI_OK;
}

/*!
	\brief 	This function unlock a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful unlocking the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_LockObjUnlock(OsiLockObj_t* pLockObj)
{
   Semaphore_post((ti_sysbios_knl_Semaphore_Handle)*pLockObj);
    return OSI_OK; /* Semaphore_post doesn't return the status */
}



/*!
	\brief 	This function call the pEntry callback from a different context

	\param	pEntry		-	pointer to the entry callback function

	\param	pValue		- 	pointer to any type of memory structure that would be
							passed to pEntry callback from the execution thread.

	\param	flags		- 	execution flags - reserved for future usage

	\return upon successful registration of the spawn the function should return 0
			(the function is not blocked till the end of the execution of the function
			and could be returned before the execution is actually completed)
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/

int osi_Spawn(P_OSI_SPAWN_ENTRY pEntry , void* pValue , unsigned long flags)
{

    tSimpleLinkSpawnMsg Msg;

    Msg.pEntry = pEntry;
    Msg.pValue = pValue;
    //Don't block if the  queue is already full.

    Mailbox_post(simpleLink_mailbox,&Msg,BIOS_NO_WAIT);
       return 0;
}

/*!
	\brief 	This is the simplelink spawn task to call SL callback from a different context

	\param	pvParameters		-	pointer to the task parameter

	\return void
	\note
	\warning
*/
void vSimpleLinkSpawnTask(void *pvParameters)
{
    tSimpleLinkSpawnMsg Msg;

    for(;;)
    {
    	Mailbox_pend(simpleLink_mailbox,&Msg,BIOS_WAIT_FOREVER);
    	Msg.pEntry(Msg.pValue);
    }
}

/*!
	\brief 	This is the API to create SL spawn task and create the SL queue

	\param	uxPriority		-	task priority

	\return void
	\note
	\warning
*/
void VStartSimpleLinkSpawnTask(unsigned long uxPriority)
{
    Task_Handle tsk_handle = NULL;
    Task_Params taskParams;
	Error_Block eb;



	Error_init(&eb);

	simpleLink_mailbox = Mailbox_create(sizeof(tSimpleLinkSpawnMsg),3,NULL,&eb);


	Task_Params_init(&taskParams);
	taskParams.stackSize = 2048;
	taskParams.priority = uxPriority;

	tsk_handle = Task_create((ti_sysbios_knl_Task_FuncPtr)vSimpleLinkSpawnTask, &taskParams, &eb);
	if (NULL == tsk_handle) {
		//return OSI_FAILURE;
	}


}
/*!
	\brief 	This function to call the memory allocation function of the RTOS

	\param	pMem		-	pointer to the memory which needs to be freed

	\return - void *
	\note
	\warning
*/

void * mem_Malloc(unsigned long Size)
{
	Error_Block eb;

	Error_init(&eb);
	if(g_GlobalHeapHdl == NULL)
	{
		HeapStd_Params params;

		//params.minBlockAlign = 8;
		HeapStd_Params_init(&params);
		params.size = 8192;
		g_GlobalHeapHdl = HeapStd_create(&params,&eb );
	}
	return HeapStd_alloc(g_GlobalHeapHdl,Size,0,&eb);
}

/*!
	\brief 	This function to call the memory de-allocation function of the RTOS

	\param	pMem		-	pointer to the memory which needs to be freed

	\return - void
	\note
	\warning
*/
void mem_Free(void *pMem)
{
	HeapStd_free(g_GlobalHeapHdl,pMem,sizeof(*pMem));
}
/*!
	\brief 	This function call the memset function
	\param	pBuf		-	pointer to the memory to be fill
    \param Val          -       Value to be fill
    \param Size         -      Size of the memory which needs to be fill

	\return - void
	\note
	\warning
*/

void  mem_set(void *pBuf,int Val,size_t Size)
{
    memset( pBuf,Val,Size);

}

/*!
	\brief 	This function call the memset function
	\param	pDst		-	pointer to the destination
    \param pSrc         -   pointer to the source
    \param Size         -   Size of the memory which needs to be copy

	\return - void
	\note
	\warning
*/
void  mem_copy(void *pDst, void *pSrc,size_t Size)
{
    memcpy(pDst,pSrc,Size);
}


/*!
	\brief 	This function use to entering into critical section
	\param	void
	\return - void
	\note
	\warning
*/

void osi_EnterCritical(void)
{
	Hwi_disable();
    //Task_disable();
    //Swi_disable();
}

/*!
	\brief 	This function use to exit critical section
	\param	void
	\return - void
	\note
	\warning
*/

void osi_ExitCritical(void)
{
    Hwi_enable();
    //Task_restore(UInt key);

}
/*!
	\brief 	This function used to create a message queue
	\param	pMsgQ	 	- pointer to the message queue
	\param	pMsgQName	- Name of the Message queue
	\param	MsgSize		- Size of msg on the message queue
	\param	MaxMsgs		- Max number of msgs on the queue
	\return - OsiReturnVal_e
	\note
	\warning
*/
OsiReturnVal_e osi_MsgQCreate(OsiMsgQ_t* 		pMsgQ ,
			      char*			pMsgQName,
			      unsigned long 		MsgSize,
			      unsigned long		MaxMsgs)
{

	*pMsgQ = (OsiMsgQ_t)Mailbox_create(MsgSize,MaxMsgs,NULL,NULL);
    return OSI_OK;
}
/*!
	\brief 	This function used to delete a message queue
	\param	pMsgQ	-	pointer to the msg queue
	\return - OsiReturnVal_e
	\note
	\warning
*/
OsiReturnVal_e osi_MsgQDelete(OsiMsgQ_t* pMsgQ)
{

	Mailbox_delete((Mailbox_Handle *)*pMsgQ);
	return OSI_OK;
}


/*!
	\brief 	This function is used to write data to the MsgQ

	\param	pMsgQ	-	pointer to the message queue
	\param	pMsg	-	pointer to the Msg strut to read into
	\param	Timeout	-	timeout to wait for the Msg to be available

	\return - OsiReturnVal_e
	\note
	\warning
*/

OsiReturnVal_e osi_MsgQWrite(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout)
{
	if(Timeout == OSI_NO_WAIT)
		Mailbox_post((ti_sysbios_knl_Mailbox_Handle)*pMsgQ,pMsg,BIOS_NO_WAIT);
	else
		Mailbox_post((ti_sysbios_knl_Mailbox_Handle)*pMsgQ,pMsg,Timeout);

	return OSI_OK;

}
/*!
	\brief 	This function is used to read data from the MsgQ

	\param	pMsgQ	-	pointer to the message queue
	\param	pMsg	-	pointer to the Msg strut to read into
	\param	Timeout	-	timeout to wait for the Msg to be available

	\return - OsiReturnVal_e
	\note
	\warning
*/

OsiReturnVal_e osi_MsgQRead(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout)
{
  if ( Timeout == OSI_WAIT_FOREVER )
    {
      Timeout = BIOS_WAIT_FOREVER ;
    }
	Mailbox_pend((ti_sysbios_knl_Mailbox_Handle)*pMsgQ,pMsg,Timeout);

	return OSI_OK;

}
/*!
	\brief 	This function used to suspend the task for the specified number of milli secs
	\param	MilliSecs	-	Time in millisecs to suspend the task
	\return - void
	\note
	\warning
*/
void osi_Sleep(unsigned int MilliSecs)
{
	UInt nticks;
	nticks = (MilliSecs / (Clock_tickPeriod/1000));
	Task_sleep(nticks);
}

/*!
	\brief 	This function used to start the kernel
	\param	void
	\return - void
	\note
	\warning
*/
void osi_start()
{

	BIOS_start();
}

/*!
	\brief 	This function used to save the os context before sleep
	\param	void
	\return - void
	\note
	\warning
*/
void osi_ContextSave()
{
}
/*!
	\brief 	This function used to retrieve the context after sleep
	\param	void
	\return - void
	\note
	\warning
*/
void osi_ContextRestore()
{
}

/* TBD*/

void SimpleLinkGeneralEventHandler(void *pArgs)
{}





/* TBD*/
void SimpleLinkSockEventHandler(void *pArgs){}





