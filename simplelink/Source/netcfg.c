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
#include "driver.h"

//*****************************************************************************
// sl_NetCfgSet
//*****************************************************************************
typedef union
{
    _NetCfgSetGet_t    Cmd;
    _BasicResponse_t   Rsp;
}_SlNetCfgMsgSet_u;

const _SlCmdCtrl_t _SlNetCfgSetCmdCtrl =
{
    SL_OPCODE_DEVICE_NETCFG_SET_COMMAND,
    sizeof(_NetCfgSetGet_t),
    sizeof(_BasicResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_NetCfgSet)
long sl_NetCfgSet(unsigned char ConfigId ,unsigned char ConfigOpt,unsigned char ConfigLen, unsigned char *pValues)
{
    _SlNetCfgMsgSet_u         Msg;
    _SlCmdExt_t               CmdExt;

	CmdExt.TxPayloadLen = (ConfigLen+3) & (~3);
    CmdExt.RxPayloadLen = 0;
    CmdExt.pTxPayload = (UINT8 *)pValues;
    CmdExt.pRxPayload = NULL;


    Msg.Cmd.ConfigId    = ConfigId;
    Msg.Cmd.ConfigLen   = ConfigLen;
	Msg.Cmd.ConfigOpt   = ConfigOpt;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetCfgSetCmdCtrl, &Msg, &CmdExt));

    return (int)Msg.Rsp.status;
}
#endif


//*****************************************************************************
// sl_NetCfg
//*****************************************************************************
typedef union
{
	_NetCfgSetGet_t	    Cmd;
	_NetCfgSetGet_t	    Rsp;
}_SlNetCfgMsgGet_u;

const _SlCmdCtrl_t _SlNetCfgGetCmdCtrl =
{
    SL_OPCODE_DEVICE_NETCFG_GET_COMMAND,
    sizeof(_NetCfgSetGet_t),
    sizeof(_NetCfgSetGet_t)
};

#if _SL_INCLUDE_FUNC(sl_NetCfgGet)
long sl_NetCfgGet(unsigned char ConfigId, unsigned char *pConfigOpt,unsigned char *pConfigLen, unsigned char *pValues)
{
    _SlNetCfgMsgGet_u         Msg;
    _SlCmdExt_t               CmdExt;

    CmdExt.TxPayloadLen = 0;
    CmdExt.RxPayloadLen = (*pConfigLen+3) & (~3);
    CmdExt.pTxPayload = NULL;
    CmdExt.pRxPayload = (UINT8 *)pValues;


    Msg.Cmd.ConfigId    = ConfigId;
    if( pConfigOpt )
    {
       Msg.Cmd.ConfigOpt   = (UINT16)*pConfigOpt;
    }
    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetCfgGetCmdCtrl, &Msg, &CmdExt));
    *pConfigLen = (UINT8)CmdExt.RxPayloadLen;//(UINT8)Msg.Rsp.ConfigLen;
     if( pConfigOpt )
     {
       *pConfigOpt = (UINT8)Msg.Rsp.ConfigOpt;
     }

    return (int)Msg.Rsp.Status;
}
#endif

