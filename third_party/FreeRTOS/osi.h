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

#ifndef __CC31xx_OSI_H__
#define	__CC31xx_OSI_H__
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#ifdef	__cplusplus
extern "C" {
#endif

#define OSI_WAIT_FOREVER   			(0xFFFFFFFF)

#define OSI_NO_WAIT        			(0)

//#define OSI_OK					(0)
  
#define OSI_FAILURE      			-1
  
typedef enum
{
  OSI_OK,
  OSI_OPERATION_FAILED = 1,
  OSI_ABORTED,
  OSI_INVALID_PARAMS,
  OSI_MEMORY_ALLOCATION_FAILURE,
  OSI_TIMEOUT,
  OSI_EVENTS_IN_USE,
  OSI_EVENT_OPEARTION_FAILURE
}OsiReturnVal_e;


#define ENTER_CRITICAL_SECTION			osi_EnterCritical()
#define EXIT_CRITICAL_SECTION			osi_ExitCritical()
  
typedef xQueueHandle OsiMsgQ_t;

 /*!
	\brief type definition for a time value

	\note	On each porting or platform the type could be whatever is needed - integer, pointer to structure etc.
*/
//typedef unsigned int OsiTime_t;
typedef portTickType OsiTime_t;
/*!
	\brief 	type definition for a sync object container

	Sync object is object used to synchronize between two threads or thread and interrupt handler.
	One thread is waiting on the object and the other thread send a signal, which then
	release the waiting thread.
	The signal must be able to be sent from interrupt context.
	This object is generally implemented by binary semaphore or events.

	\note	On each porting or platform the type could be whatever is needed - integer, structure etc.
*/
//typedef unsigned int OsiSyncObj_t;
typedef xSemaphoreHandle OsiSyncObj_t;

/*!
	\brief 	type definition for a locking object container

	Locking object are used to protect a resource from mutual accesses of two or more threads.
	The locking object should suppurt reentrant locks by a signal thread.
	This object is generally implemented by mutex semaphore

	\note	On each porting or platform the type could be whatever is needed - integer, structure etc.
*/
//typedef unsigned int OsiLockObj_t;
typedef xSemaphoreHandle OsiLockObj_t;

/*!
	\brief 	type definition for a spawn entry callback

	the spawn mechanism enable to run a function on different context.
	This mechanism allow to transfer the execution context from interrupt context to thread context
	or changing teh context from an unknown user thread to general context.
	The implementation of the spawn mechanism depends on the user's system reqeuirements and could varies
	from implementation of serialized execution using signle thread to creating thread per call

	\note	The stack size of the execution thread must be at least of TBD bytes!
*/
typedef void (*P_OSI_SPAWN_ENTRY)(void* pValue);

typedef void (*P_OSI_EVENT_HANDLER)(void* pValue);

typedef void (*P_OSI_TASK_ENTRY)(void* pValue);

typedef void (*P_OSI_INTR_ENTRY)(void* pValue);

typedef void* OsiTaskHandle;

/*!
	\brief 	This function registers an interrupt in NVIC table

	The sync object is used for synchronization between diffrent thread or ISR and
	a thread.

	\param	iIntrNum	-	Interrupt number to register
	\param	pEntry	    -	Pointer to the interrupt handler

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_InterruptRegister(int iIntrNum,P_OSI_INTR_ENTRY pEntry);

/*!
	\brief 	This function Deregisters an interrupt in NVIC table

	\param	iIntrNum	-	Interrupt number to register
	\param	pEntry	    -	Pointer to the interrupt handler

	\return upon successful creation the function should return Postive number
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
void osi_InterruptDeRegister(int iIntrNum);


/*!
	\brief 	This function creates a sync object

	The sync object is used for synchronization between diffrent thread or ISR and
	a thread.

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjCreate(OsiSyncObj_t* pSyncObj); //raghu added char* pName


/*!
	\brief 	This function deletes a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjDelete(OsiSyncObj_t* pSyncObj);

/*!
	\brief 		This function generates a sync signal for the object.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signaling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function could be called from ISR context
	\warning
*/
int osi_SyncObjSignal(OsiSyncObj_t* pSyncObj);

/*!
	\brief 		This function generates a sync signal for the object.
				from ISR context.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signaling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function is called from ISR context
	\warning
*/
int osi_SyncObjSignalFromISR(OsiSyncObj_t* pSyncObj);

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
int osi_SyncObjWait(OsiSyncObj_t* pSyncObj , OsiTime_t Timeout);

/*!
	\brief 	This function clears a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful clearing the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_SyncObjClear(OsiSyncObj_t* pSyncObj);

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
int osi_LockObjCreate(OsiLockObj_t* pLockObj); 

/*!
	\brief 	This function deletes a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_LockObjDelete(OsiLockObj_t* pLockObj);

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
int osi_LockObjLock(OsiLockObj_t* pLockObj , OsiTime_t Timeout);

/*!
	\brief 	This function unlock a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful unlocking the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_LockObjUnlock(OsiLockObj_t* pLockObj);


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
/*!
	\brief 	This function creates a Task.

	Creates a new Task and add it to the last of tasks that are ready to run

	\param	pEntry	-	pointer to the Task Function
	\param	pcName	-	Task Name String
	\param	usStackDepth	-	Stack Size
	\param	pvParameters	-	pointer to structure to be passed to the Task Function
	\param	uxPriority	-	Task Priority

	\return upon successful creation the function should return 1
			Otherwise, 0 or a negative value indicating the error code shall be returned
	\note
	\warning
*/
int osi_TaskCreate(P_OSI_TASK_ENTRY pEntry,const signed char * const pcName,unsigned short usStackDepth,void *pvParameters,unsigned long uxPriority,OsiTaskHandle pTaskHandle);

/*!
	\brief 	This function Deletes a Task.

	Deletes a  Task and remove it from list of running task

	\param	pTaskHandle	-	Task Handle

	\note
	\warning
*/
void osi_TaskDelete(OsiTaskHandle pTaskHandle);

int osi_Spawn(P_OSI_SPAWN_ENTRY pEntry , void* pValue , unsigned long flags);


/*******************************************************************************

This function creates a message queue that is typically used for inter thread
communication. 

Parameters:

	pMsgQ		-	pointer to the message queue control block
	pMsgQName	-	pointer to the name of the message queue
	MsgSize		-	the size of the message. 

			NOTICE: THE MESSGAE SIZE MUST BE SMALLER THAN 16

	MaxMsgs		-	maximum number of messages.

Please note that this function allocates the entrie memory requiered 
for the maximum number of messages (MsgSize * MaxMsgs). 

********************************************************************************/
OsiReturnVal_e osi_MsgQCreate(OsiMsgQ_t* 		pMsgQ , 
							  char*				pMsgQName,
							  unsigned long 		MsgSize,
							  unsigned long 		MaxMsgs);

/*******************************************************************************

This function deletes a specific message queue.
All threads suspended waiting for a message from this queue are resumed with
an error return value. 

Parameters:

	pMsgQ		-	pointer to the message queue control block

********************************************************************************/
OsiReturnVal_e osi_MsgQDelete(OsiMsgQ_t* pMsgQ);


/*******************************************************************************

This function writes a message to a specific message queue.

Notice that the message is copied to the queue from the memory area specified 
by pMsg pointer.

--------------------------------------------------------------------------------
THIS FUNCTION COULD BE CALLED FROM ISR AS LONG AS THE TIMEOUT PARAMETER IS 
SET TO "OSI_NO_WAIT"
--------------------------------------------------------------------------------

Parameters:

	pMsgQ		-	pointer to the message queue control block
	pMsg		- 	pointer to the message
	Timeout		-	numeric value specifies the maximum number of mSec to stay 
					suspended while waiting for available space for the message

********************************************************************************/
OsiReturnVal_e osi_MsgQWrite(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout);


/*******************************************************************************

This function retrieves a message from the specified message queue. The
retrieved message is copied from the queue into the memory area specified by 
the pMsg pointer 

Parameters:

	pMsgQ		-	pointer to the message queue control block
	pMsg		- 	pointer that specify the location where to copy the message
	Timeout		-	numeric value specifies the maximum number of mSec to stay 
					suspended while waiting for a message to be available

********************************************************************************/
OsiReturnVal_e osi_MsgQRead(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout);

void osi_start();
void * mem_Malloc(unsigned long Size);



void mem_Free(void *pMem);


/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning        
*/
void  mem_set(void *pBuf,int Val,size_t Size);

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning        
*/
void  mem_copy(void *pDst, void *pSrc,size_t Size);

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning        
*/
void osi_EnterCritical(void);

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning        
*/
void osi_ExitCritical(void);



/*!
	\brief structure definition for simple link spawn message

	\note	On each porting or platform the type could be whatever is needed - integer, pointer to structure etc.
*/
  
typedef struct
{
    P_OSI_SPAWN_ENTRY pEntry;
    void* pValue;
}tSimpleLinkSpawnMsg;
  
/* The queue used to send message to simple link spwan task. */
extern xQueueHandle xSimpleLinkSpawnQueue;

/* API for SL Task*/
void VStartSimpleLinkSpawnTask(unsigned portBASE_TYPE uxPriority);




#ifdef  __cplusplus
}
#endif // __cplusplus

#endif
