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
#ifndef __CDN_CLIENT_H__
#define __CDN_CLIENT_H__

#ifdef    __cplusplus
extern "C" {
#endif

#define ALIGNMENT_READ_MASK 0x0F

/* RunStatus */
#define CDN_STATUS_DOWNLOAD_DONE        1
#define CDN_STATUS_OK                   0
#define CDN_STATUS_CONTINUE             0
#define CDN_STATUS_ERROR                -1
#define CDN_STATUS_ERROR_OPEN_FILE      -2
#define CDN_STATUS_ERROR_RECV_CHUNK     -3
#define CDN_STATUS_ERROR_SAVE_CHUNK     -4
#define CDN_STATUS_ERROR_MAX_EAGAIN     -5
#define CDN_STATUS_ERROR_CONNECT_CDN    -6
#define CDN_STATUS_ERROR_READ_HDRS      -7

typedef enum
{
    CDN_STATE_IDLE = 0,   
    CDN_STATE_CDN_SERVER_CONNECTED, 
    CDN_STATE_FILE_DOWNLOAD_AND_SAVE,                    
    CDN_STATE_CLOSED
} CdnState_e;

/* locals - move to object control block */
typedef struct 
{
    char cdn_server_name[64]; /* save server name in order not to connect every file (if same server) */
    _i32  port_num;

    char *p_file_name;
    _i32 file_size;
    _u32 ulToken;
    _i32 lFileHandle;
    _i16 fileSockId;
    char *recv_buf;

    /* states */
    CdnState_e  state;
    _i32 totalBytesReceived;
    OtaFileMetadata_t *pResourceMetadata;
    FlcCb_t    *pFlcSflashCb;
    FlcCb_t    *pFlcHostCb;
    FlcCb_t    *pFlcCb;
} CdnClient_t;

void *CdnClient_Init(FlcCb_t *pFlcSflashCb, FlcCb_t *pFlcHostCb);
_i32  CdnClient_ConnectByUrl(void *pvCdnClient, OtaFileMetadata_t *pResourceMetadata);
_i32  CdnClient_SetFileInfo(void *pvCdnClient, _i32 file_size, char *file_name);
_i32  CdnClient_Run(void *pvCdnClient);
void CdnClient_CloseServer(void *pvCdnClient);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __CDN_CLIENT_H__ */
