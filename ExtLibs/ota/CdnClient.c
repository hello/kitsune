/*
 * CdnClient.c - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
#include "simplelink.h"7
#include "OtaCommon.h"
#include "ota_api.h"
#include "OtaHttp.h"
#include "CdnClient.h"

/* Internals */
CdnClient_t g_CdnClient;

_i32 _OpenStorageFile(CdnClient_t *pCdnClient, char *file_name, _i32 file_size, _u32 *ulToken, _i32 *lFileHandle);
_i32 _ReadFileHeaders(_i16 fileSockId, char *domian_name, char *file_name);
_i32 _RecvFileChunk(_i16 fileSockId, void *pBuf, _i32 Len, _i32 flags);
_i32 _readSignature(CdnClient_t *pCdnClient, char *signature_filename, OtaFileMetadata_t *pMetadata);

void *CdnClient_Init(FlcCb_t *pFlcSflashCb, FlcCb_t *pFlcHostCb)
{
    CdnClient_t *pCdnClient = &g_CdnClient;

    memset(pCdnClient, 0, sizeof(CdnClient_t));
    pCdnClient->fileSockId = -1;
    pCdnClient->port_num = SOCKET_PORT_DEFAULT;
    pCdnClient->pFlcSflashCb = pFlcSflashCb;
    pCdnClient->pFlcHostCb = pFlcHostCb;
    pCdnClient->pFlcCb = pFlcSflashCb; /* default is SFLASH */
    pCdnClient->totalBytesReceived = 0;
    pCdnClient->state = CDN_STATE_IDLE;

    return (void *)pCdnClient;
}


_i32 CdnClient_ConnectByUrl(void *pvCdnClient, OtaFileMetadata_t *pResourceMetadata)
{
    CdnClient_t *pCdnClient = (CdnClient_t *)pvCdnClient;
    char domain_name[64];
    char req_uri[128];
    char *cdn_url = pResourceMetadata->cdn_url;
    _i32 status;

    /* extract CDN server name and file name */
    status = http_extract_domain_by_url((char *)cdn_url, domain_name, req_uri);
    if (status < 0)
    {
        UARTprintf("CdnClient_ConnectByUrl: ERROR, http_extract_domain_by_url, status=%d\n", status);
        return OTA_STATUS_ERROR;
    }

    /* Skip connect server if same CDN server */
    if (strcmp(pCdnClient->cdn_server_name, domain_name) != 0)
    {
        /* close previous CDN server socket */
        CdnClient_CloseServer(pCdnClient);
        strcpy(pCdnClient->cdn_server_name, domain_name);
        /* connect to the CDN server */
        pCdnClient->port_num = SOCKET_PORT_DEFAULT;
        pCdnClient->fileSockId = http_connect_server(pCdnClient->cdn_server_name, 0, pCdnClient->port_num, SOCKET_SECURED, SOCKET_BLOCKING);
        if (pCdnClient->fileSockId < 0)
        {
            UARTprintf("CdnClient_ConnectByUrl: ERROR on http_connect_server, status=%d\n", pCdnClient->fileSockId);
            CdnClient_CloseServer(pvCdnClient);
            if (pCdnClient->fileSockId == OTA_STATUS_ERROR_CONTINUOUS_ACCESS)
            {
                return OTA_STATUS_ERROR_CONTINUOUS_ACCESS;
            }
            return CDN_STATUS_ERROR_CONNECT_CDN;
        }
    }

    /* read file headers, skip all not relenant headers */
    status = _ReadFileHeaders(pCdnClient->fileSockId, pCdnClient->cdn_server_name, req_uri); 
    if (status < 0)
    {
        UARTprintf("CdnClient_ConnectByUrl: ERROR, _ReadFileHeaders, status=%d\n", status);
        CdnClient_CloseServer(pvCdnClient);
        return CDN_STATUS_ERROR_READ_HDRS;
    }

    pCdnClient->totalBytesReceived = 0;
    pCdnClient->state = CDN_STATE_CDN_SERVER_CONNECTED;

    if (pResourceMetadata->flags & METADATA_FLAGS_NOT_SFLASH_STORAGE)
    {
        /* Host storage */
        pCdnClient->pFlcCb = pCdnClient->pFlcHostCb; /* switch (only for current file) to host storage  */
    }
    else
    {
        /* Sflash storage */
        pCdnClient->pFlcCb = pCdnClient->pFlcSflashCb; /* switch back to default SFLASH storage  */
    }

    /* save metadata */
    pCdnClient->pResourceMetadata = pResourceMetadata;

    return OTA_STATUS_OK;
}

_i32 _ReadFileHeaders(_i16 fileSockId, char *domian_name, char *file_name)
{
    _i32 status=0;
    _i32 len;
    char *send_buf = http_send_buf();

    UARTprintf("_ReadFileHeaders: domain=%s, file=%s\n", domian_name, file_name);
    http_build_request (send_buf, "GET ", domian_name, NULL, file_name, NULL, NULL);  

    len = sl_Send(fileSockId, send_buf, (_i16)strlen(send_buf), 0);
    if (len <= 0)
    {
        UARTprintf("_ReadFileHeaders: ERROR, sl_Send, status=%d\n", len);
        return OTA_STATUS_ERROR;
    }

    UARTprintf("_ReadFileHeaders: skip http headers\n");
    status = http_skip_headers(fileSockId);
    if (status < 0)
    {
        UARTprintf("_ReadFileHeaders: ERROR, http_skip_headers, status=%d\n", status);
        return OTA_STATUS_ERROR;
    }

    return OTA_STATUS_OK;
}

_i32 _RecvFileChunk(_i16 fileSockId, void *pBuf, _i32 Len, _i32 flags)
{
    _i32 len;

    len = sl_Recv_eagain(fileSockId, pBuf, Len, flags, MAX_EAGAIN_RETRIES);

    return len;
}

/* default file access is SFLASH, user should overwrite this functions in order to save on Host files */
_i32 _readSignature(CdnClient_t *pCdnClient, char *signature_filename, OtaFileMetadata_t *pMetadata)
{
    FlcCb_t *pFlcCb = pCdnClient->pFlcCb;
    _i32 lFileHandle;
    _i32 status;
    _i32 len;

    status = pFlcCb->pOpenFile((_u8 *)signature_filename , 0, NULL, &lFileHandle, _FS_MODE_OPEN_READ);
    if(status < 0)
    {
        UARTprintf("_readSignature: Error pOpenFile\n");
        return OTA_STATUS_ERROR;
    }

    len = pFlcCb->pReadFile(lFileHandle , 0, pMetadata->signature, sizeof(pMetadata->signature));
    if(len < 0)
    {
        UARTprintf("_readSignature: Error pReadFile\n");
        return OTA_STATUS_ERROR;
    }
    pMetadata->p_signature = pMetadata->signature;
    pMetadata->signature_len = len;

    status = pFlcCb->pCloseFile(lFileHandle, NULL, NULL, 0);
    if(status < 0)
    {
        UARTprintf("_readSignature: Error pCloseFile\n");
        return OTA_STATUS_ERROR;
    }
    return OTA_STATUS_OK;
}

/* default file access is SFLASH, user should overwrite this functions in order to save on Host files */
_i32 _OpenStorageFile(CdnClient_t *pCdnClient, char *file_name, _i32 file_size, _u32 *ulToken, _i32 *lFileHandle)
{
    OtaFileMetadata_t *pMetadata = pCdnClient->pResourceMetadata;
    _i32 fs_open_flags=0;
    _i32 status;

    if((int)(pCdnClient->pFlcCb) == 0)
    {
        pCdnClient->state = CDN_STATE_IDLE;
        UARTprintf("_OpenStorageFile: pCdnClient->pFlcCb is NULL !!!!\n");
        return OTA_STATUS_ERROR;
    }
    /*  create a user file with mirror */
    fs_open_flags  = _FS_FILE_OPEN_FLAG_COMMIT;

    /* set security flags */
    if (pMetadata->flags & METADATA_FLAGS_SECURED)
    {
        fs_open_flags |= _FS_FILE_PUBLIC_WRITE;
        fs_open_flags |= _FS_FILE_OPEN_FLAG_SECURE;
        if ((pMetadata->flags & METADATA_FLAGS_SIGNATURE) == 0)
        {
            fs_open_flags |= _FS_FILE_OPEN_FLAG_NO_SIGNATURE_TEST;
        }
        else
        {
            status = _readSignature(pCdnClient, pMetadata->signature_filename, pMetadata);
            if (status < 0)
            {
                UARTprintf( "_OpenStorageFile: error in _readSignature, status=%d\n", status);
                return OTA_STATUS_ERROR;
            }
        }
    }

    status = pCdnClient->pFlcCb->pOpenFile((_u8 *)file_name, file_size, ulToken, lFileHandle, fs_open_flags);
    if(status < 0)
    {
        UARTprintf( "_OpenStorageFile: error in pOpenFile, status=%d\n", status);
        return OTA_STATUS_ERROR;
    }
    return OTA_STATUS_OK;
}

void CdnClient_CloseServer(void *pvCdnClient)
{
    CdnClient_t *pCdnClient = (CdnClient_t *)pvCdnClient;

    pCdnClient->cdn_server_name[0] = 0;        
    if (pCdnClient->fileSockId >= 0)
    {
        sl_Close(pCdnClient->fileSockId);
        pCdnClient->fileSockId = -1;
    }
}

_i32 CdnClient_Run(void *pvCdnClient)
{
    CdnClient_t *pCdnClient = (CdnClient_t *)pvCdnClient;
    FlcCb_t *pFlcCb = pCdnClient->pFlcCb;
    _i32 status;
    _i32 bytesReceived;
    char *endBuf;

    /*UARTprintf("sl_extLib_CdnRun: entry, state=%d, totalBytesReceived=%d\n", pCdnClient->state, pCdnClient->totalBytesReceived); */

    switch (pCdnClient->state)
    {
        case CDN_STATE_CDN_SERVER_CONNECTED:
            UARTprintf( "CdnClient_Run: Create/Open for write file %s\n", pCdnClient->p_file_name);
    
            /*  create a user file */
            status = _OpenStorageFile(pCdnClient, pCdnClient->p_file_name, pCdnClient->file_size, &pCdnClient->ulToken, &pCdnClient->lFileHandle);
            if(status < 0)
            {
                pCdnClient->state = CDN_STATE_IDLE;
                UARTprintf("CdnClient_Run ERROR: pCdnClient->pOpenFileCB\n");
                return CDN_STATUS_ERROR_OPEN_FILE;
            }

            pCdnClient->state = CDN_STATE_FILE_DOWNLOAD_AND_SAVE;
            UARTprintf( "CdnClient_Run: file opened\n");
            pCdnClient->recv_buf = http_recv_buf();
            memset(pCdnClient->recv_buf, 0, sizeof(HTTP_RECV_BUF_LEN));
            break;
            
        case CDN_STATE_FILE_DOWNLOAD_AND_SAVE:
            
            bytesReceived = _RecvFileChunk(pCdnClient->fileSockId, &pCdnClient->recv_buf[0], HTTP_RECV_BUF_LEN, 0);
            if (0 >= bytesReceived)
            {
                UARTprintf("CdnClient_Run: ERROR sl_Recv status=%d\n", bytesReceived);
                /*!!!!Don't close the file do Abort or just reset without close */
                /*!!!!pFlcCb->pCloseFile(pCdnClient->lFileHandle, NULL, NULL, 0); */
                pFlcCb->pAbortFile(pCdnClient->lFileHandle);
                pCdnClient->state = CDN_STATE_IDLE;
                if (bytesReceived == SL_EAGAIN)
                    return CDN_STATUS_ERROR_MAX_EAGAIN;
                return CDN_STATUS_ERROR_RECV_CHUNK;

            }

            /* End of file, ignore "HTTP/1.1" ... */
            endBuf = pCdnClient->recv_buf;
            if ((pCdnClient->totalBytesReceived + bytesReceived) >= pCdnClient->file_size)
            {
                bytesReceived = pCdnClient->file_size - pCdnClient->totalBytesReceived;
                endBuf = pCdnClient->recv_buf+1; /* sign EOF  */
            }
            else
            {
                /* Force align length */
                if (bytesReceived & ALIGNMENT_READ_MASK)
                {
                    status = _RecvFileChunk(pCdnClient->fileSockId, &pCdnClient->recv_buf[bytesReceived], 
                                          (ALIGNMENT_READ_MASK + 1) - (bytesReceived & ALIGNMENT_READ_MASK), 0);
                    if (0 >= status)
                    {
                        UARTprintf("CdnClient_Run: ERROR sl_Recv on force align recv status=%d\n", status);
                        /*!!!!Don't close the file do Abort or just reset without close */
                        /*!!!!pFlcCb->pCloseFile(pCdnClient->lFileHandle, NULL, NULL, 0); */
                        pFlcCb->pAbortFile(pCdnClient->lFileHandle);
                        pCdnClient->state = CDN_STATE_IDLE;
                        if (status == SL_EAGAIN)
                            return CDN_STATUS_ERROR_MAX_EAGAIN;
                        return CDN_STATUS_ERROR_RECV_CHUNK;

                    }
                    bytesReceived += status;
                }
            }


            /* Write the packet to the file */
            UARTprintf( "CdnClient_Run: Write size %d to file %s total %d.\n", bytesReceived, pCdnClient->p_file_name, pCdnClient->totalBytesReceived );
            status = pFlcCb->pWriteFile((_i32)pCdnClient->lFileHandle, pCdnClient->totalBytesReceived, pCdnClient->recv_buf, bytesReceived); 
            if (status < 0)
            {
                UARTprintf("CdnClient_Run: Error - SaveFileChunk - (%d)\n", status);
                /*!!!!Don't close the file do Abort or just reset without close */
                /*!!!!pFlcCb->pCloseFile(pCdnClient->lFileHandle, NULL, NULL, 0); */
                pFlcCb->pAbortFile(pCdnClient->lFileHandle);
                pCdnClient->state = CDN_STATE_IDLE;
                return CDN_STATUS_ERROR_SAVE_CHUNK;
            }

            pCdnClient->totalBytesReceived += bytesReceived;

            /* check EOF */
            if (endBuf > pCdnClient->recv_buf)
            {
                UARTprintf("CdnClient_Run: End of file\n");
                status = pFlcCb->pCloseFile(pCdnClient->lFileHandle, (_u8 *)pCdnClient->pResourceMetadata->p_cert_filename , (_u8 *)pCdnClient->pResourceMetadata->p_signature, pCdnClient->pResourceMetadata->signature_len);
                if (status < 0)
                {
                    UARTprintf( "CdnClient_Run: error on pCloseFile, status=%d\n", status);
                    /*!!!!Don't close the file do Abort or just reset without close */
                    /*!!!!pFlcCb->pCloseFile(pCdnClient->lFileHandle, NULL, NULL, 0); */
                    pFlcCb->pAbortFile(pCdnClient->lFileHandle);
                    pCdnClient->state = CDN_STATE_IDLE;
                    return OTA_STATUS_ERROR;
                }

               
                UARTprintf("CdnClient_Run: Downloading File Completed - Size=%d\n", pCdnClient->totalBytesReceived);
                pCdnClient->state = CDN_STATE_IDLE;
                return CDN_STATUS_DOWNLOAD_DONE;

            }

            break;
        
        default:
            UARTprintf("CdnClient_Run ERROR: unexpected state %d\n", pCdnClient->state);
            pCdnClient->state = CDN_STATE_IDLE;
            return CDN_STATUS_ERROR;

    }

    return CDN_STATUS_CONTINUE;
}

_i32 CdnClient_SetFileInfo(void *pvCdnClient, _i32 file_size, char *file_name)
{
    CdnClient_t *pCdnClient = (CdnClient_t *)pvCdnClient;
    pCdnClient->p_file_name = file_name;
    pCdnClient->file_size = file_size;
    
    return OTA_STATUS_OK;
}
