/*
 * LogClient.c - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
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
#include "simplelink.h"
#include "OtaCommon.h"
#include "ota_api.h"
#include "OtaApp.h"
#include "OtaHttp.h"
#include "LogClient.h"

/* Internals */
LogClient_t g_LogClient;

void *LogClient_Init(void)
{
    LogClient_t *pLogClient = &g_LogClient;

    memset(pLogClient, 0, sizeof(LogClient_t));
    pLogClient->serverSockId = -1;

    return (void *)pLogClient;
}

_i32 LogClient_ConnectServer(void *pvLogClient, OtaOptServerInfo_t *pOtaServerInfo)
{
    LogClient_t *pLogClient = (LogClient_t *)pvLogClient;

    pLogClient->pOtaServerInfo = pOtaServerInfo;
    /* Connect to the OTA server */
    UARTprintf("LogClient_ConnectServer: http_connect_server %s\n", pOtaServerInfo->log_server_name);
    pLogClient->serverSockId = http_connect_server(pOtaServerInfo->log_server_name, 0, SOCKET_PORT_DEFAULT, SOCKET_SECURED, SOCKET_BLOCKING);
    if (pLogClient->serverSockId < 0)
    {
        UARTprintf("LogClient_ConnectServer: ERROR http_connect_server, status=%d\n", pLogClient->serverSockId);
        if (pLogClient->serverSockId == OTA_STATUS_ERROR_CONTINUOUS_ACCESS)
        {
            return OTA_STATUS_ERROR_CONTINUOUS_ACCESS;
        }
        return OTA_STATUS_ERROR;
    }

    return OTA_STATUS_OK;
}

_i32 LogClient_Print(void *pvLogClient, char *pFilename, OtaApp_statistics_t *pStatistics)
{
    LogClient_t *pLogClient = (LogClient_t *)pvLogClient;
    _i32 status=0;
    _i32 len;
    char *send_buf = http_send_buf();
    char *response_buf = http_recv_buf();
    char local_buf[96];
    char *log_filename = local_buf;
    char *log_buf = http_recv_buf();

    strcpy(log_filename, "/Log/Stat_");
    strcat(log_filename, pFilename);
    strcat(log_filename, ".txt");
    /*strcat(log_filename, "?overwrite=false"); */

    http_build_request (send_buf, "PUT ", pLogClient->pOtaServerInfo->log_server_name, pLogClient->pOtaServerInfo->rest_files_put, log_filename, pLogClient->pOtaServerInfo->rest_hdr, pLogClient->pOtaServerInfo->rest_hdr_val);

    /* prepare log buffer */
    sprintf(log_buf, "start counter=%d, access errors=%d ---------------\n", pStatistics->startCount, pStatistics->continuousAccessErrorCount);
    sprintf(&log_buf[strlen(log_buf)], "connect server done=%d, error=%d, update=%d, metadata=%d\n", pStatistics->ota_connect_done, pStatistics->ota_connect_error, pStatistics->ota_error_updatecheck, pStatistics->ota_error_metadata);
    sprintf(&log_buf[strlen(log_buf)], "connect cdn    done=%d, error=%d\n", pStatistics->cdn_connect_done, pStatistics->cdn_connect_error);
    sprintf(&log_buf[strlen(log_buf)], "download file  done=%d, error=%d, save_chunk=%d, max_eagain=%d\n", pStatistics->download_done, pStatistics->download_error, pStatistics->download_error_save_chunk, pStatistics->download_error_max_eagain);

    /* Add file length header */
    send_buf[strlen(send_buf)-2]=0;
    sprintf(&send_buf[strlen(send_buf)], "Content-Length: %d", strlen(log_buf));
    /*strcat(send_buf, "Content-Length: 76"); */
    strcat(send_buf, "\r\n");
    strcat(send_buf, "\r\n");

    len = sl_Send(pLogClient->serverSockId, send_buf, (_i16)strlen(send_buf), 0);
    if (len <= 0)
    {
        UARTprintf("LogClient_UpdateCheck: ERROR metadata sl_Send status=%d\n", len);
        return OTA_STATUS_ERROR;
    }

    len = sl_Send(pLogClient->serverSockId, log_buf, (_i16)strlen(log_buf), 0);
    if (len <= 0)
    {
        UARTprintf("LogClient_UpdateCheck: ERROR metadata sl_Send status=%d\n", len);
        return OTA_STATUS_ERROR;
    }

    /* recv json */
    len = sl_Recv_eagain(pLogClient->serverSockId, response_buf, HTTP_RECV_BUF_LEN, 0, MAX_EAGAIN_RETRIES);
    if (len <= 0)
    {
            UARTprintf("LogClient_UpdateCheck: ERROR metadata sl_Recv status=%d\n", status);
            return OTA_STATUS_ERROR;
    }

    return OTA_STATUS_OK;
}

void LogClient_Close(void *pvLogClient)
{
    LogClient_t *pLogClient = (LogClient_t *)pvLogClient;

    if (pLogClient->serverSockId >= 0)
    {
        sl_Close(pLogClient->serverSockId);
        pLogClient->serverSockId = -1;
    }
}

_i32 LogClient_ConnectAndPrint(void *pvLogClient, OtaOptServerInfo_t *pOtaServerInfo, char *vendorStr, OtaApp_statistics_t *pStatistics)
{
    _i32 status;
    char filename[40];
    _u8 *pMac = pOtaServerInfo->log_mac_address;
    

    status = LogClient_ConnectServer(pvLogClient, pOtaServerInfo);
    if( status < 0)
    {
        UARTprintf("LogClient_ConnectAndPrint: LogClient_ConnectServer, status=%d\n", status);
        return status;
    }

    sprintf(filename,"%s_Mac%02X%02X%02X%02X%02X%02X", vendorStr, pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5]);
    LogClient_Print(pvLogClient, filename, pStatistics);
    LogClient_Close(pvLogClient);
    return OTA_STATUS_OK;
}
