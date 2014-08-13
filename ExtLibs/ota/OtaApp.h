/*
 * OtaApp.h - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
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
#ifndef __OTA_APP_H__
#define __OTA_APP_H__

#ifdef    __cplusplus
extern "C" {
#endif

typedef enum
{
    OTA_STATE_IDLE = 0,   
    OTA_STATE_CONNECT_SERVER, 
    OTA_STATE_RESOURCE_LIST,                    
    OTA_STATE_METADATA,
    OTA_STATE_CONNECT_CDN,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_WAIT_CONFIRM,
    OTA_STATE_NUMBER_OF_STATES
} OtaState_e;

typedef struct 
{
    char vendorStr[20];
    _i32 startCount;
    _i32 continuousAccessErrorCount;
    
    _i32 ota_connect_done;
    _i32 ota_connect_error;
    _i32 ota_error_updatecheck;
    _i32 ota_error_metadata;

    _i32 cdn_connect_error;
    _i32 cdn_connect_done;
    _i32 download_error;
    _i32 download_error_connect_cdn;
    _i32 download_error_read_hdrs;
    _i32 download_error_save_chunk;
    _i32 download_error_max_eagain;
    _i32 download_done;
} OtaApp_statistics_t; 

typedef struct 
{
    OtaState_e state;
    _i32 runMode;
    char vendorStr[20];
    OtaOptServerInfo_t *pOtaServerInfo;
    OtaFileMetadata_t  *pResourceMetadata;
    OtaApp_statistics_t  *pStatistics;
    _i32 resetNwp;
    _i32 resetMcu;

    void *pvOtaClient;
    void *pvCdnClient;
    void *pvLogClient;

    /* updateCheck info */
    _i32  numUpdates;
    char *file_path;
    _i32  file_size;
} OtaApp_t; 

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __OTA_APP_H__ */
