/*
 * CdnClient.h - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
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
#ifndef __LOG_CLIENT_H__
#define __LOG_CLIENT_H__

#ifdef    __cplusplus
extern "C" {
#endif

/* locals - move to object control block */
typedef struct 
{
    OtaOptServerInfo_t       *pOtaServerInfo;
    _i16 serverSockId;
} LogClient_t;

void *LogClient_Init(void);
void LogClient_Close(void *pvLogClient);
_i32  LogClient_ConnectServer(void *pvLogClient, OtaOptServerInfo_t *pOtaServerInfo);
_i32  LogClient_Print(void *pvLogClient, char *vendorStr, OtaApp_statistics_t *pStatistics);
_i32  LogClient_ConnectAndPrint(void *pvLogClient, OtaOptServerInfo_t *pOtaServerInfo, char *vendorStr, OtaApp_statistics_t *pStatistics);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __LOG_CLIENT_H__ */
