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

#ifndef __USER_H__
#define __USER_H__

#ifdef  __cplusplus
extern "C" {
#endif

#include <string.h>  
#include "cc3101_pal.h"


typedef Fd_t                                    _SlFd_t;

/*!
 ******************************************************************************

    \defgroup       porting_user_include        Porting - User Include Files

    This section IS NOT REQUIRED in case user provided primitives are handled 
    in makefiles or project configurations (IDE) 

    PORTING ACTION: 
        - Include all required header files for the definition of:
            -# Transport layer library API (e.g. SPI, UART)
            -# OS primitives definitions (e.g. Task spawn, Semaphores)
            -# Memory management primitives (e.g. alloc, free)

 ******************************************************************************
 */

/*!
	\def		MAX_CONCURRENT_ACTIONS

    \brief      Defines the maximum number of concurrent action in the system
				Min:1 , Max: 32
                    
                Actions which has async events as return, can be 

    \sa             

    \note       In case there are not enough resources for the actions needed in the system,
	        	error is received: POOL_IS_EMPTY
			    one option is to increase MAX_CONCURRENT_ACTIONS 
				(improves performance but results in memory consumption)
		     	Other option is to call the API later (decrease performance)

    \warning    In case of setting to one, recommend to use non-blocking recv\recvfrom to allow
				multiple socket recv
*/
#define MAX_CONCURRENT_ACTIONS 10
/*!
 ******************************************************************************

    \defgroup       proting_capabilities        Porting - Capabilities Set

    This section IS NOT REQUIRED in case one of the following pre defined 
    capabilities set is in use:
    - SL_TINY
    - SL_SMALL
    - SL_FULL

    PORTING ACTION: 
        - Define one of the pre-defined capabilities set or uncomment the
          relevant definitions below to select the required capabilities

    @{

 *******************************************************************************
*/

/*!
	\def		SL_INC_ARG_CHECK

    \brief      Defines whether the SimpleLink driver perform argument check 
                or not

                When defined, the SimpleLink driver perform argument check on 
                function call. Removing this define could reduce some code 
                size and improve slightly the performances but may impact in 
                unpredictable behavior in case of invalid arguments

    \sa

    \note       belongs to \ref proting_sec

    \warning    Removing argument check may cause unpredictable behavior in 
                case of invalid arguments. 
                In this case the user is responsible to argument validity 
                (for example all handlers must not be NULL)
*/
#define SL_INC_ARG_CHECK


/*!
    \def		SL_INC_STD_BSD_API_NAMING

    \brief      Defines whether SimpleLink driver should expose standard BSD 
                APIs or not

                When defined, the SimpleLink driver in addtion to its alternative
                BSD APIs expose also standard BSD APIs.
    Stadrad BSD API includs the following functions:
                socket , close , accept , bind , listen	, connect , select , 
                setsockopt	, getsockopt , recv , recvfrom , write , send , sendto , 
                gethostbyname

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/

#define SL_INC_STD_BSD_API_NAMING


/*!
    \brief      Defines whether to include extended API in SimpleLink driver
                or not

                When defined, the SimpleLink driver will include also all 
                exteded API of the included packages

    \sa             ext_api

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_EXT_API

/*!
    \brief      Defines whether to include WLAN package in SimpleLink driver 
                or not

                When defined, the SimpleLink driver will include also 
                the WLAN package

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_WLAN_PKG

/*!
    \brief      Defines whether to include SOCKET package in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also 
                the SOCKET package

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_SOCKET_PKG

/*!
    \brief      Defines whether to include NET_APP package in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also the 
                NET_APP package

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_NET_APP_PKG

/*!
    \brief      Defines whether to include NET_CFG package in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also 
                the NET_CFG package

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_NET_CFG_PKG

/*!
    \brief      Defines whether to include NVMEM package in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also the 
                NVMEM package

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_NVMEM_PKG

/*!
    \brief      Defines whether to include socket server side APIs 
                in SimpleLink driver or not

                When defined, the SimpleLink driver will include also socket 
                server side APIs

    \sa             server_side

    \note       

    \warning
*/
#define SL_INC_SOCK_SERVER_SIDE_API

/*!
    \brief      Defines whether to include socket client side APIs in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also socket 
                client side APIs

    \sa             client_side

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_SOCK_CLIENT_SIDE_API

/*!
    \brief      Defines whether to include socket receive APIs in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also socket 
                receive side APIs

    \sa             recv_api

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_SOCK_RECV_API

/*!
    \brief      Defines whether to include socket send APIs in SimpleLink 
                driver or not

                When defined, the SimpleLink driver will include also socket 
                send side APIs

    \sa             send_api

    \note       belongs to \ref proting_sec

    \warning
*/
#define SL_INC_SOCK_SEND_API

/*!

 Close the Doxygen group.
 @}

 */


/*!
 ******************************************************************************

    \defgroup   porting_enable_device       Porting - Device Enable/Disable

    The enable/disable line (nHib) provide mechanism to enter the device into
    the least current consumption mode. This mode could be used when no traffic
    is required (tx/rx).
    when this hardware line is not connected to any IO of the host this define
    should be left empty.


    \note   Not connecting this line results in ability to start the driver
            only once.
    
    PORTING ACTION:
        - Bind the GPIO that is connected to the device to the SimpleLink
          driver

    @{

 ******************************************************************************
*/

/*!
    \brief		Enable the device by set the appropriate GPIO to high

    \sa			sl_DeviceDisable

    \note           belongs to \ref proting_sec

    \warning	if nHib/nShutdown pins are not connected to the host this define
     	 	 	should be left empty. Not connecting on of these lines may result
     	 	 	in higher power consumption and inability to start and stop the
     	 	 	driver correctly.
*/
#define sl_DeviceEnable()			NwpPowerOn()

/*!
    \brief		Disable the device by setting the appropriate GPIO to Low

    \sa			sl_DeviceEnable

    \note           belongs to \ref proting_sec

    \warning	if nHib/nShutdown pins are not connected to the host this define
     	 	 	should be left empty. Not connecting on of these lines may result
     	 	 	in higher power consumption and inability to start and stop the
     	 	 	driver correctly.
*/
#define sl_DeviceDisable() 			NwpPowerOff()

/*!

 Close the Doxygen group.
 @}

 */

/*!
 ******************************************************************************

    \defgroup   porting_interface         Porting - Communication Interface

    The simple link device can work with different communication
    channels (e.g. spi/uart). Texas Instruments provides single driver
    that can work with all these types. This section bind between the
    physical communication interface channel and the SimpleLink driver


    \note       Correct and efficient implementation of this driver is critical
                for the performances of the SimpleLink device on this platform.


    PORTING ACTION:
        - Bind the functions of the communication channel interface driver with
          the simple link driver

    @{

 ******************************************************************************
*/

#define _SlFd_t					Fd_t

/*!
    \brief      Opens an interface communication port to be used for communicating
                with a SimpleLink device
	
	            Given an interface name and option flags, this function opens 
                the communication port and creates a file descriptor. 
                This file descriptor is used afterwards to read and write 
                data from and to this specific communication channel.
	            The speed, clock polarity, clock phase, chip select and all other 
                specific attributes of the channel are all should be set to hardcoded
                in this function.
	
	\param	 	ifName  -   points to the interface name/path. The interface name is an 
                            optional attributes that the simple link driver receives
                            on opening the driver (sl_Start). 
                            In systems that the spi channel is not implemented as 
                            part of the os device drivers, this parameter could be NULL.

	\param      flags   -   optional flags parameters for future use

	\return     upon successful completion, the function shall open the channel 
                and return a non-negative integer representing the file descriptor.
                Otherwise, -1 shall be returned 
					
    \sa         sl_IfClose , sl_IfRead , sl_IfWrite

	\note       The prototype of the function is as follow:
                    Fd_t xxx_IfOpen(char* pIfName , unsigned long flags);

    \note       belongs to \ref proting_sec

    \warning
*/
#define sl_IfOpen                           spi_Open

/*!
    \brief      Closes an opened interface communication port
	
	\param	 	fd  -   file descriptor of opened communication channel

	\return		upon successful completion, the function shall return 0. 
			    Otherwise, -1 shall be returned 
					
    \sa         sl_IfOpen , sl_IfRead , sl_IfWrite

	\note       The prototype of the function is as follow:
                    int xxx_IfClose(Fd_t Fd);

    \note       belongs to \ref proting_sec

    \warning
*/
#define sl_IfClose                          spi_Close

/*!
    \brief      Attempts to read up to len bytes from an opened communication channel 
                into a buffer starting at pBuff.
	
	\param	 	fd      -   file descriptor of an opened communication channel
	
	\param		pBuff   -   pointer to the first location of a buffer that contains enough 
                            space for all expected data

	\param      len     -   number of bytes to read from the communication channel

	\return     upon successful completion, the function shall return the number of read bytes. 
                Otherwise, 0 shall be returned 
					
    \sa         sl_IfClose , sl_IfOpen , sl_IfWrite


	\note       The prototype of the function is as follow:
                    int xxx_IfRead(Fd_t Fd , char* pBuff , int Len);

    \note       belongs to \ref proting_sec

    \warning
*/
#define sl_IfRead                           spi_Read

/*!
    \brief attempts to write up to len bytes to the SPI channel
	
	\param	 	fd      -   file descriptor of an opened communication channel
	
	\param		pBuff   -   pointer to the first location of a buffer that contains 
                            the data to send over the communication channel

	\param      len     -   number of bytes to write to the communication channel

	\return     upon successful completion, the function shall return the number of sent bytes. 
				therwise, 0 shall be returned 
					
    \sa         sl_IfClose , sl_IfOpen , sl_IfRead

	\note       This function could be implemented as zero copy and return only upon successful completion
                of writing the whole buffer, but in cases that memory allocation is not too tight, the 
                function could copy the data to internal buffer, return back and complete the write in 
                parallel to other activities as long as the other SPI activities would be blocked until 
                the entire buffer write would be completed 

               The prototype of the function is as follow:
                    int xxx_IfWrite(Fd_t Fd , char* pBuff , int Len);

    \note       belongs to \ref proting_sec

    \warning
*/
#define sl_IfWrite                          spi_Write

/*!
    \brief 		register an interrupt handler routine for the host IRQ

	\param	 	InterruptHdl	-	pointer to interrupt handler routine

	\param 		pValue			-	pointer to a memory structure that is passed
									to the interrupt handler.

	\return		upon successful registration, the function shall return 0.
				Otherwise, -1 shall be returned

    \sa

	\note		If there is already registered interrupt handler, the function
				should overwrite the old handler with the new one

	\note       If the handler is a null pointer, the function should un-register the
	            interrupt handler, and the interrupts can be disabled.

    \note       belongs to \ref proting_sec

    \warning
*/
#define sl_IfRegIntHdlr(InterruptHdl , pValue)          NwpRegisterInterruptHandler(InterruptHdl , pValue)   

/*!
    \brief 		Masks the Host IRQ

    \sa		sl_IfUnMaskIntHdlr



    \note       belongs to \ref proting_sec

    \warning
*/


#define sl_IfMaskIntHdlr()								NwpMaskInterrupt()

/*!
    \brief 		Unmasks the Host IRQ

    \sa		sl_IfMaskIntHdlr



    \note       belongs to \ref proting_sec

    \warning
*/

#define sl_IfUnMaskIntHdlr()								NwpUnMaskInterrupt()

    

/*!

 Close the Doxygen group.
 @}

*/

/*!
 ******************************************************************************

    \defgroup   porting_mem_mgm             Porting - Memory Management

    This section declare in which memory management model the SimpleLink driver
    will run:
        -# Static
        -# Dynamic

    This section IS NOT REQUIRED in case Static model is selected.

    The default memory model is Static

    PORTING ACTION:
        - If dynamic model is selected, define the alloc and free functions.

    @{

 *****************************************************************************
*/

/*!
    \brief      Defines whether the SimpleLink driver is working in dynamic
                memory model or not

                When defined, the SimpleLink driver use dynamic allocations
                if dynamic allocation is selected malloc and free functions
                must be retrieved

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/
/*
#define SL_MEMORY_MGMT_DYNAMIC
*/

#ifdef SL_MEMORY_MGMT_DYNAMIC

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning        
*/
#define sl_Malloc(Size)                                 malloc(Size)

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning        
*/
#define sl_Free(pMem)                                   free(pMem)

                        
#endif

/*!

 Close the Doxygen group.
 @}

*/

/*!
 ******************************************************************************

    \defgroup   porting_os          Porting - Operating System

    The simple link driver can run on multi-threaded environment as well
    as non-os environment (mail loop)

    This section IS NOT REQUIRED in case you are working on non-os environment.

    If you choose to work in multi-threaded environment under any operating system
    you will have to provide some basic adaptation routines to allow the driver
    to protect access to resources from different threads (locking object) and
    to allow synchronization between threads (sync objects).

    PORTING ACTION:
        -# Uncomment SL_PLATFORM_MULTI_THREADED define
        -# Bind locking object routines
        -# Bind synchronization object routines
        -# Optional - Bind spawn thread routine

    @{

 ******************************************************************************
*/

#define SL_PLATFORM_MULTI_THREADED

#ifdef SL_PLATFORM_MULTI_THREADED
#include "osi.h"

typedef OsiSyncObj_t                            _SlSyncObj_t;
typedef OsiLockObj_t                            _SlLockObj_t;

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning
*/
#define SL_OS_RET_CODE_OK                       ((int)OSI_OK)

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning
*/
#define SL_OS_WAIT_FOREVER                      ((OsiTime_t)OSI_WAIT_FOREVER)

/*!
    \brief
    \sa
    \note           belongs to \ref proting_sec
    \warning
*/
#define SL_OS_NO_WAIT	                        ((OsiTime_t)OSI_NO_WAIT)

/*!
	\brief type definition for a time value

	\note	On each porting or platform the type could be whatever is needed - integer, pointer to structure etc.

    \note       belongs to \ref proting_sec
*/
#define _SlTime_t				OsiTime_t

/*!
	\brief 	type definition for a sync object container

	Sync object is object used to synchronize between two threads or thread and interrupt handler.
	One thread is waiting on the object and the other thread send a signal, which then
	release the waiting thread.
	The signal must be able to be sent from interrupt context.
	This object is generally implemented by binary semaphore or events.

	\note	On each porting or platform the type could be whatever is needed - integer, structure etc.

    \note       belongs to \ref proting_sec
*/
//#define _SlSyncObj_t			

    
/*!
	\brief 	This function creates a sync object

	The sync object is used for synchronization between diffrent thread or ISR and
	a thread.

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned

    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_SyncObjCreate(pSyncObj,pName)            osi_SyncObjCreate(pSyncObj)


/*!
	\brief 	This function deletes a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_SyncObjDelete(pSyncObj)                  osi_SyncObjDelete(pSyncObj)


/*!
	\brief 		This function generates a sync signal for the object.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signaling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function could be called from ISR context
    \warning
*/
#define sl_SyncObjSignal(pSyncObj)                osi_SyncObjSignal(pSyncObj)  

/*!
	\brief 		This function generates a sync signal for the object.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signaling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function could be called from ISR context
    \warning
*/
#define sl_SyncObjSignalFromISR(pSyncObj)                osi_SyncObjSignalFromISR(pSyncObj)

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
    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_SyncObjWait(pSyncObj,Timeout)            osi_SyncObjWait(pSyncObj,Timeout)  

/*!
	\brief 	type definition for a locking object container

	Locking object are used to protect a resource from mutual accesses of two or more threads.
	The locking object should suppurt reentrant locks by a signal thread.
	This object is generally implemented by mutex semaphore

	\note	On each porting or platform the type could be whatever is needed - integer, structure etc.
    \note       belongs to \ref proting_sec
*/
//#define _SlLockObj_t 			

/*!
	\brief 	This function creates a locking object.

	The locking object is used for protecting a shared resources between different
	threads.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_LockObjCreate(pLockObj,pName)            osi_LockObjCreate(pLockObj)

/*!
	\brief 	This function deletes a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_LockObjDelete(pLockObj)                  osi_LockObjDelete(pLockObj)

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
    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_LockObjLock(pLockObj,Timeout)           osi_LockObjLock(pLockObj,Timeout) 

/*!
	\brief 	This function unlock a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful unlocking the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
    \note       belongs to \ref proting_sec
    \warning
*/
#define sl_LockObjUnlock(pLockObj)                   osi_LockObjUnlock(pLockObj) 


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
    \note       belongs to \ref proting_sec
    \warning
*/
#define SL_PLATFORM_EXTERNAL_SPAWN

#ifdef SL_PLATFORM_EXTERNAL_SPAWN
#define sl_Spawn(pEntry,pValue,flags)       osi_Spawn(pEntry,pValue,flags)        
#endif

#endif
/*!

 Close the Doxygen group.
 @}

 */


/*!
 ******************************************************************************

    \defgroup       porting_events      Porting - Event Handlers

    This section includes the asynchronous event handlers routines

    PORTING ACTION:
        -Uncomment the required handler and define your routine as the value
        of this handler

    @{

 ******************************************************************************
 */

/*!
    \brief

    \sa

    \note       belongs to \ref proting_sec

    \warning
*/

//#define sl_GeneralEvtHdlr


/*!
    \brief          An event handler for WLAN connection or disconnection indication

    \sa

    \note           belongs to \ref proting_sec

    \warning
*/

#define sl_WlanEvtHdlr                     SimpleLinkWlanEventHandler         


/*!
    \brief          An event handler for IP address asynchronous event. Usually accepted after new WLAN connection

    \sa

    \note           belongs to \ref proting_sec

    \warning
*/

#define sl_NetAppEvtHdlr              		SimpleLinkNetAppEventHandler              


/*!
    \brief

    \sa

    \note           belongs to \ref proting_sec

    \warning
*/



#define sl_HttpServerCallback 			SimpleLinkHttpServerCallback

/*!

 Close the Doxygen group.
 @}

 */


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif // __USER_H__
