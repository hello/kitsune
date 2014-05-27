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


#define sl_min(a,b) (((a) < (b)) ? (a) : (b))
#define MAX_NVMEM_CHUNK_SIZE  1460

//*****************************************************************************
// 
//*****************************************************************************
typedef union
{
	_FsOpenCommand_t	    Cmd;
	_FsOpenResponse_t	    Rsp;
}_SlFsOpenMsg_u;

const _SlCmdCtrl_t _SlFsOpenCmdCtrl =
{
    SL_OPCODE_NVMEM_FILEOPEN,
    sizeof(_FsOpenCommand_t),
    sizeof(_FsOpenResponse_t)
};

unsigned short sl_Strlen(const unsigned char *buffer)
{
    unsigned short len = 0;
    while(*buffer++) len++;
    return len;
}


#if _SL_INCLUDE_FUNC(sl_FsOpen)
long sl_FsOpen(unsigned char *pFileName,unsigned long AccessModeAndMaxSize, unsigned long *pToken,long *pFileHandle)
{
    _SlReturnVal_t        RetVal;
    _SlFsOpenMsg_u        Msg;
    _SlCmdExt_t           CmdExt;

    CmdExt.TxPayloadLen = (sl_Strlen(pFileName)+4) & (~3); // add 4: 1 for NULL and the 3 for align 
    CmdExt.RxPayloadLen = 0;
    CmdExt.pTxPayload = pFileName;
    CmdExt.pRxPayload = NULL;

    Msg.Cmd.Mode          =  AccessModeAndMaxSize; 
    Msg.Cmd.Token         = *pToken;

    RetVal = _SlDrvCmdOp((_SlCmdCtrl_t *)&_SlFsOpenCmdCtrl, &Msg, &CmdExt);
    *pFileHandle = Msg.Rsp.FileHandle;
    *pToken =      Msg.Rsp.Token;

	// in case of an error, return the erros file handler as an error code
	if( *pFileHandle < 0 )
	{
	   return *pFileHandle;
	}
    return (int)RetVal;
}
#endif

//*****************************************************************************
// 
//*****************************************************************************
typedef union
{
	_FsCloseCommand_t	    Cmd;
	_BasicResponse_t	    Rsp;
}_SlFsCloseMsg_u;

const _SlCmdCtrl_t _SlFsCloseCmdCtrl =
{
    SL_OPCODE_NVMEM_FILECLOSE,
    sizeof(_FsCloseCommand_t),
    sizeof(_FsCloseResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_FsClose)
int sl_FsClose(long FileHdl,unsigned char CertificateFileId,unsigned char SignatureLen,unsigned char Signature)
{
    _SlFsCloseMsg_u Msg;

    Msg.Cmd.FileHandle        = FileHdl;
    Msg.Cmd.certificateFileId = CertificateFileId;
    Msg.Cmd.SignatureLen      = SignatureLen;
    Msg.Cmd.Signature         = Signature;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlFsCloseCmdCtrl, &Msg, NULL));

    return (int)Msg.Rsp.status;
}
#endif

//*****************************************************************************
// 
//*****************************************************************************
typedef union
{
	_FsReadCommand_t	    Cmd;
	_FsReadResponse_t	    Rsp;
}_SlFsReadMsg_u;

const _SlCmdCtrl_t _SlFsReadCmdCtrl =
{
    SL_OPCODE_NVMEM_FILEREADCOMMAND,
    sizeof(_FsReadCommand_t),
    sizeof(_FsReadResponse_t)
};

 
#if _SL_INCLUDE_FUNC(sl_FsRead)
long sl_FsRead(long FileHdl, unsigned long Offset, unsigned char* pData, unsigned long Len)
{
    _SlFsReadMsg_u      Msg;
    _SlCmdExt_t         ExtCtrl;
    unsigned short      ChunkLen;
    _SlReturnVal_t      RetVal =0;
    long                RetCount = 0;

    ExtCtrl.TxPayloadLen = 0;
    ExtCtrl.pTxPayload   = NULL;

    ChunkLen = (unsigned short)sl_min(MAX_NVMEM_CHUNK_SIZE,Len);
    ExtCtrl.RxPayloadLen = ChunkLen;
    ExtCtrl.pRxPayload   = (UINT8 *)(pData);
    Msg.Cmd.Offset       = Offset;
    Msg.Cmd.Len          = ChunkLen;
    Msg.Cmd.FileHandle   = FileHdl;
    do
    {
        RetVal = _SlDrvCmdOp((_SlCmdCtrl_t *)&_SlFsReadCmdCtrl, &Msg, &ExtCtrl);
        if(SL_OS_RET_CODE_OK == RetVal)
        {
            RetCount += (long)Msg.Rsp.status;
            Len -= ChunkLen;
            Offset += ChunkLen;
            Msg.Cmd.Offset      = Offset;
            ExtCtrl.pRxPayload   += ChunkLen;
            ChunkLen = (unsigned short)sl_min(MAX_NVMEM_CHUNK_SIZE,Len);
            ExtCtrl.RxPayloadLen  = ChunkLen;
            Msg.Cmd.Len           = ChunkLen;
            Msg.Cmd.FileHandle  = FileHdl;
        }
        else
        {
            return RetVal;
        }
    }while(ChunkLen > 0);

    return (long)RetCount;
}
#endif

//*****************************************************************************
// 
//*****************************************************************************
typedef union
{
	_FsWriteCommand_t	    Cmd;
	_FsWriteResponse_t	    Rsp;
}_SlFsWriteMsg_u;

const _SlCmdCtrl_t _SlFsWriteCmdCtrl =
{
    SL_OPCODE_NVMEM_FILEWRITECOMMAND,
    sizeof(_FsWriteCommand_t),
    sizeof(_FsWriteResponse_t)
};



#if _SL_INCLUDE_FUNC(sl_FsWrite)
long sl_FsWrite(long FileHdl, unsigned long Offset, unsigned char* pData, unsigned long Len)
{
    _SlFsWriteMsg_u     Msg;
    _SlCmdExt_t         ExtCtrl;
    unsigned short      ChunkLen;
    _SlReturnVal_t      RetVal;
    long                RetCount = 0;

    ExtCtrl.RxPayloadLen = 0;
    ExtCtrl.pRxPayload   = NULL;

    ChunkLen = (unsigned short)sl_min(MAX_NVMEM_CHUNK_SIZE,Len);
    ExtCtrl.TxPayloadLen = ChunkLen;
    ExtCtrl.pTxPayload   = (UINT8 *)(pData);
    Msg.Cmd.Offset      = Offset;
    Msg.Cmd.Len          = ChunkLen;
    Msg.Cmd.FileHandle  = FileHdl;

    do
    {
    
        RetVal = _SlDrvCmdOp((_SlCmdCtrl_t *)&_SlFsWriteCmdCtrl, &Msg, &ExtCtrl);
        if(SL_OS_RET_CODE_OK == RetVal)
        {
            RetCount += (INT32)RetVal;
            Len -= ChunkLen;
            Offset += ChunkLen;
            Msg.Cmd.Offset        = Offset;
            ExtCtrl.pTxPayload   += ChunkLen;
            ChunkLen = (unsigned short)sl_min(MAX_NVMEM_CHUNK_SIZE,Len);
            ExtCtrl.TxPayloadLen  = ChunkLen;
            Msg.Cmd.Len           = ChunkLen;
            Msg.Cmd.FileHandle  = FileHdl;
        }
        else
        {
            return RetVal;
        }
    }while(ChunkLen > 0);

    return (long)RetCount;
}
#endif

//*****************************************************************************
// 
//*****************************************************************************
typedef union
{
	_FsGetInfoCommand_t	    Cmd;
	_FsGetInfoResponse_t    Rsp;
}_SlFsGetInfoMsg_u;

const _SlCmdCtrl_t _SlFsGetInfoCmdCtrl =
{
    SL_OPCODE_NVMEM_FILEGETINFOCOMMAND,
    sizeof(_FsGetInfoCommand_t),
    sizeof(_FsGetInfoResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_FsGetInfo)
int sl_FsGetInfo(unsigned char *pFileName,unsigned long Token,SlFsFileInfo_t* pFsFileInfo)
{
    _SlFsGetInfoMsg_u    Msg;
    _SlCmdExt_t          CmdExt;

    CmdExt.TxPayloadLen = (sl_Strlen(pFileName)+4) & (~3); // add 4: 1 for NULL and the 3 for align 
    CmdExt.RxPayloadLen = 0;
    CmdExt.pTxPayload   = pFileName;
    CmdExt.pRxPayload   = NULL;
    Msg.Cmd.Token       = Token;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlFsGetInfoCmdCtrl, &Msg, &CmdExt));

    pFsFileInfo->flags        = Msg.Rsp.flags;
    pFsFileInfo->FileLen      = Msg.Rsp.FileLen;
    pFsFileInfo->AllocatedLen = Msg.Rsp.AllocatedLen;
    pFsFileInfo->Token[0]     = Msg.Rsp.Token[0];
    pFsFileInfo->Token[1]     = Msg.Rsp.Token[1];
    pFsFileInfo->Token[2]     = Msg.Rsp.Token[2];
    return (int)Msg.Rsp.Status;
}
#endif

//*****************************************************************************
// 
//*****************************************************************************
typedef union
{
	_FsDeleteCommand_t   	    Cmd;
	_FsDeleteResponse_t	        Rsp;
}_SlFsDeleteMsg_u;

const _SlCmdCtrl_t _SlFsDeleteCmdCtrl =
{
    SL_OPCODE_NVMEM_FILEDELCOMMAND,
    sizeof(_FsDeleteCommand_t),
    sizeof(_FsDeleteResponse_t)
};

#if _SL_INCLUDE_FUNC(sl_FsDel)
int sl_FsDel(unsigned char *pFileName,unsigned long Token)
{
    _SlFsDeleteMsg_u Msg;
    _SlCmdExt_t          CmdExt;

    CmdExt.TxPayloadLen = (sl_Strlen(pFileName)+4) & (~3); // add 4: 1 for NULL and the 3 for align 
    CmdExt.RxPayloadLen = 0;
    CmdExt.pTxPayload   = pFileName;
    CmdExt.pRxPayload   = NULL;
    Msg.Cmd.Token       = Token;


    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlFsDeleteCmdCtrl, &Msg, &CmdExt));

    return (int)Msg.Rsp.status;
}
#endif
