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

/*****************************************************************************

    API Prototypes

 *****************************************************************************/
#include "datatypes.h"
#include "simplelink.h"
#include "protocol.h"
#include "flowcont.h"
#include "driver.h"


//*****************************************************************************
//
//*****************************************************************************
#if _SL_INCLUDE_FUNC(sl_Task)
void sl_Task()
{
#ifdef _SlTaskEntry
    _SlTaskEntry();
#endif
}
#endif

//*****************************************************************************
//
//*****************************************************************************
#if _SL_INCLUDE_FUNC(sl_Start)
int sl_Start(const void* pIfHdl, const char* pDevName, const P_SL_DEV_START_STOP_CALLBACK pInitCallBack)
{
	int	pObjIdx = MAX_CONCURRENT_ACTIONS;
	InitComplete_t  AsyncRsp;

    // callback init
    _SlDrvDriverCBInit();

    // open the interface: usually SPI or UART
    if (NULL == pIfHdl)
    {
        g_pCB->FD = sl_IfOpen((char *)pDevName, 0);
    }
    else
    {
        g_pCB->FD = *(_SlFd_t *)pIfHdl;
    }
	//Use Obj to issue the command, if not available try later
	pObjIdx = _SlDrvWaitForPoolObj(START_STOP_ID,SL_MAX_SOCKETS);
	if (MAX_CONCURRENT_ACTIONS == pObjIdx)
 	{
		return POOL_IS_EMPTY;
	}
	g_pCB->ObjPool[pObjIdx].pRespArgs = (UINT8 *)&AsyncRsp;

    if( g_pCB->FD >= 0)
    {
        sl_DeviceDisable();

        sl_IfRegIntHdlr((SL_P_EVENT_HANDLER)_SlDrvRxIrqHandler, NULL);

        sl_DeviceEnable();

        if (NULL != pInitCallBack)
        {
            g_pCB->pInitCallback = pInitCallBack;
        }
        else
        {
			OSI_RET_OK_CHECK(sl_SyncObjWait(&g_pCB->ObjPool[pObjIdx].SyncObj, SL_OS_WAIT_FOREVER));
			if (INIT_STA_OK == AsyncRsp.Status)
			{
				return ROLE_STA;
			}
			else if (INIT_STA_ERR == AsyncRsp.Status)
			{
				return ROLE_STA_ERR;
			}
			else if (INIT_AP_OK == AsyncRsp.Status)
			{
				return ROLE_AP;
			}
			else if (INIT_AP_ERR == AsyncRsp.Status)
			{
				return ROLE_AP_ERR;
			}
			else if (INIT_P2P_OK == AsyncRsp.Status)
			{
				return ROLE_P2P;
			}
			else if (INIT_P2P_ERR == AsyncRsp.Status)
			{
				return ROLE_P2P_ERR;
			}
		}
    }

    return (int)g_pCB->FD;
 
}
#endif

//*****************************************************************************
// _sl_HandleAsync_InitComplete - handles init complete signalling to 
//                                a waiting object
//*****************************************************************************
void _sl_HandleAsync_InitComplete(void *pVoidBuf)
{
	InitComplete_t     *pMsgArgs   = (InitComplete_t *)_SL_RESP_ARGS_START(pVoidBuf);

	 memcpy(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs, pMsgArgs, sizeof(InitComplete_t));

    if(g_pCB->pInitCallback)
    {
		g_pCB->pInitCallback(pMsgArgs->Status);
    }
    else
    {
        OSI_RET_OK_CHECK(sl_SyncObjSignal(&g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].SyncObj));
    }
	//release Pool Object
	_SlDrvReleasePoolObj(g_pCB->FunctionParams.AsyncExt.ActionIndex);
}

void _sl_HandleAsync_Stop(void *pVoidBuf)
{
    _BasicResponse_t     *pMsgArgs   = (_BasicResponse_t *)_SL_RESP_ARGS_START(pVoidBuf);

    VERIFY_SOCKET_CB(NULL != g_pCB->StopCB.pAsyncRsp);

    memcpy(g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].pRespArgs, pMsgArgs, sizeof(_BasicResponse_t));

	OSI_RET_OK_CHECK(sl_SyncObjSignal(&g_pCB->ObjPool[g_pCB->FunctionParams.AsyncExt.ActionIndex].SyncObj));
	//release Pool Object
	_SlDrvReleasePoolObj(g_pCB->FunctionParams.AsyncExt.ActionIndex);

    return;
}


//*****************************************************************************
// sl_stop
//*****************************************************************************
typedef union
{
	_DevStopCommand_t  Cmd;
	_BasicResponse_t   Rsp;    
}_SlStopMsg_u;

const _SlCmdCtrl_t _SlStopCmdCtrl =
{
    SL_OPCODE_DEVICE_STOP_COMMAND,
    sizeof(_DevStopCommand_t),
    sizeof(_BasicResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_Stop)
int sl_Stop(unsigned short timeout)
{
    int RetVal;
    _SlStopMsg_u      Msg;
    _BasicResponse_t  AsyncRsp;
	int				  pObjIdx = MAX_CONCURRENT_ACTIONS;
    // if timeout is 0 the shutdown is forced immediately
    if( 0 == timeout ) 
    {
      sl_IfRegIntHdlr(NULL, NULL);
      sl_DeviceDisable();
      RetVal = sl_IfClose(g_pCB->FD);

    }
    else
    {
      // let the device make the shutdown using the defined timeout
      Msg.Cmd.Timeout = timeout;
	  //Use Obj to issue the command, if not available try later
	  pObjIdx = _SlDrvWaitForPoolObj(START_STOP_ID,SL_MAX_SOCKETS);
	  if (MAX_CONCURRENT_ACTIONS == pObjIdx)
 	  {
		return POOL_IS_EMPTY;
	  }
	  g_pCB->ObjPool[pObjIdx].pRespArgs = (UINT8 *)&AsyncRsp;

      VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlStopCmdCtrl, &Msg, NULL));

      if(SL_OS_RET_CODE_OK == (int)Msg.Rsp.status)
      {
         OSI_RET_OK_CHECK(sl_SyncObjWait(&g_pCB->ObjPool[pObjIdx].SyncObj, SL_OS_WAIT_FOREVER));
         Msg.Rsp.status = AsyncRsp.status;
         RetVal = Msg.Rsp.status;
      }
	  else
	  {
	  	_SlDrvReleasePoolObj(pObjIdx);
	  }

      sl_IfRegIntHdlr(NULL, NULL);
      sl_DeviceDisable();
      sl_IfClose(g_pCB->FD);
    }
    _SlDrvDriverCBDeinit();

    return RetVal;
}
#endif


//*****************************************************************************
// sl_EventMaskSet/sl_EventMaskGet
//*****************************************************************************
typedef union
{
	_DevMaskEventSetCommand_t	    Cmd;
	_BasicResponse_t	            Rsp;
}_SlEventMaskSetMsg_u;

const _SlCmdCtrl_t _SlEventMaskSetCmdCtrl =
{
    SL_OPCODE_DEVICE_EVENTMASKSET,
    sizeof(_DevMaskEventSetCommand_t),
    sizeof(_BasicResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_EventMaskSet)
int sl_EventMaskSet(unsigned char EventClass , unsigned long Mask)
{
    _SlEventMaskSetMsg_u Msg;

    Msg.Cmd.group = EventClass;
    Msg.Cmd.mask = Mask;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlEventMaskSetCmdCtrl, &Msg, NULL));

    return (int)Msg.Rsp.status;
}
#endif

//*****************************************************************************
//
//*****************************************************************************
typedef union
{
	_DevMaskEventGetCommand_t	    Cmd;
	_DevMaskEventGetResponse_t      Rsp;
}_SlEventMaskGetMsg_u;

const _SlCmdCtrl_t _SlEventMaskGetCmdCtrl =
{
    SL_OPCODE_DEVICE_EVENTMASKGET,
    sizeof(_DevMaskEventGetCommand_t),
    sizeof(_DevMaskEventGetResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_EventMaskGet)
int sl_EventMaskGet(unsigned char EventClass, unsigned long *pMask)
{
    _SlEventMaskGetMsg_u Msg;

    Msg.Cmd.group = EventClass;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlEventMaskGetCmdCtrl, &Msg, NULL));

    *pMask = Msg.Rsp.mask;
    return SL_RET_CODE_OK;
}
#endif



//*****************************************************************************
// sl_DeviceGet
//*****************************************************************************

typedef union
{
	_DeviceSetGet_t	    Cmd;
	_DeviceSetGet_t	    Rsp;
}_SlDeviceMsgGet_u;

const _SlCmdCtrl_t _SlDeviceGetCmdCtrl =
{
	SL_OPCODE_DEVICE_DEVICEGET,
	sizeof(_DeviceSetGet_t),
	sizeof(_DeviceSetGet_t)
};



#if _SL_INCLUDE_FUNC(sl_DeviceGet)
long sl_DeviceGet(unsigned char DeviceGetId, unsigned char *pOption,unsigned char *pConfigLen, unsigned char *pValues)
{
	_SlDeviceMsgGet_u         Msg;
	_SlCmdExt_t               CmdExt;

	if( pOption )
	{
	  CmdExt.TxPayloadLen = 0;
	  CmdExt.RxPayloadLen = (*pConfigLen+3) & (~3);
	  CmdExt.pTxPayload = NULL;
	  CmdExt.pRxPayload = (UINT8 *)pValues;
  
	  Msg.Cmd.DeviceSetId = DeviceGetId;

	  Msg.Cmd.Option   = (UINT16)*pOption;
	  
	  VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlDeviceGetCmdCtrl, &Msg, &CmdExt));
	  
	  *pConfigLen = (UINT8)CmdExt.RxPayloadLen;
  
	  if( pOption )
	  {
		  *pOption = (UINT8)Msg.Rsp.Option;
	  }
  
	  return (int)Msg.Rsp.Status;
	}
    else
    {
		return -1;
    }
}
#endif

//*****************************************************************************
// sl_DeviceSet
//*****************************************************************************
typedef union
{
    _DeviceSetGet_t    Cmd;
    _BasicResponse_t   Rsp;
}_SlDeviceMsgSet_u;

const _SlCmdCtrl_t _SlDeviceSetCmdCtrl =
{
    SL_OPCODE_DEVICE_DEVICESET,
    sizeof(_DeviceSetGet_t),
    sizeof(_BasicResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_DeviceSet)
long sl_DeviceSet(unsigned char DeviceSetId ,unsigned char Option,unsigned char ConfigLen, unsigned char *pValues)
{
	_SlDeviceMsgSet_u         Msg;
	_SlCmdExt_t               CmdExt;

	CmdExt.TxPayloadLen = (ConfigLen+3) & (~3);
	CmdExt.RxPayloadLen = 0;
	CmdExt.pTxPayload = (UINT8 *)pValues;
	CmdExt.pRxPayload = NULL;


	Msg.Cmd.DeviceSetId    = DeviceSetId;
	Msg.Cmd.ConfigLen   = ConfigLen;
	Msg.Cmd.Option   = Option;

	VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlDeviceSetCmdCtrl, &Msg, &CmdExt));

	return (int)Msg.Rsp.status;
}
#endif




//*****************************************************************************
//  _SlDrvDeviceEventHandler - handles internally device async events
//*****************************************************************************
void _SlDrvDeviceEventHandler(void *pArgs)
{
    _SlResponseHeader_t      *pHdr       = (_SlResponseHeader_t *)pArgs;

    switch(pHdr->GenHeader.Opcode)
    {
        case SL_OPCODE_DEVICE_INITCOMPLETE:
            _sl_HandleAsync_InitComplete(pHdr);
            break;
        case SL_OPCODE_DEVICE_STOP_ASYNC_RESPONSE:
            _sl_HandleAsync_Stop(pHdr);
            break;
        case  SL_OPCODE_DEVICE_DEVICEASYNCFATALERROR:
#ifdef sl_GeneralEvtHdlr
            {
              _BasicResponse_t     *pMsgArgs   = (_BasicResponse_t *)_SL_RESP_ARGS_START(pHdr);
              SlDeviceEvent_t      devHandler;
              devHandler.EventData.deviceEvent.status = pMsgArgs->status & 0xFF;
              devHandler.EventData.deviceEvent.sender = (pMsgArgs->status >> 8) & 0xFF;
              sl_GeneralEvtHdlr((void*)&devHandler);
            }
#endif
            break;
        default:
            SL_ERROR_TRACE2(MSG_306, "ASSERT: _SlDrvDeviceEventHandler : invalid opcode = 0x%x = %1", pHdr->GenHeader.Opcode, pHdr->GenHeader.Opcode);
            VERIFY_PROTOCOL(0);
    }
}


