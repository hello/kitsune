/*
 * OtaClient.h - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
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
#ifndef __OTA_CLIENT_H__
#define __OTA_CLIENT_H__

#ifdef    __cplusplus
extern "C" {
#endif

typedef struct  
{
    char filename[64];  
    _i32  size;
} RsrcData_t; 

typedef struct 
{
    _i32  status;
    _i32  num_updates;
    _i32  vendor_id;
    RsrcData_t rsrcList[MAX_RESOURCES];
} OtaCheckUpdateResponse_t; 

#define METADATA_FLAGS_SECURED              0x00000001
#define METADATA_FLAGS_SIGNATURE            0x00000002
#define METADATA_FLAGS_CERTIFICATE          0x00000004
#define METADATA_FLAGS_NOT_CONVERT_SYS_DIR  0x00000008
#define METADATA_FLAGS_NOT_SFLASH_STORAGE   0x00000010
#define METADATA_FLAGS_RESEREVED_1          0x00000020
#define METADATA_FLAGS_RESET_NWP            0x00000040
#define METADATA_FLAGS_RESET_MCU            0x00000080

typedef struct 
{
    OtaOptServerInfo_t       *pOtaServerInfo;
    OtaCheckUpdateResponse_t *pOtaCheckUpdateResponse;
    OtaFileMetadata_t        *pOtaMetadataResponse;
    FlcCb_t           *pFlcSflashCb;
    FlcCb_t           *pFlcHostCb;
    _i16  serverSockId;
    _i32  currUpdateIndex;
    _i32  numUpdates;
    char *pVendorStr;
} OtaClient_t; 

void *OtaClient_Init(FlcCb_t *pFlcSflashCb, FlcCb_t *pFlcHostCb);
_i32   OtaClient_ConnectServer(void *pvOtaClient, OtaOptServerInfo_t *pOtaServerInfo);
_i32   OtaClient_UpdateCheck(void *pvOtaClient, char *pVendorStr);
char *OtaClient_GetNextUpdate(void *pvOtaClient, _i32 *size);
_i32   OtaClient_ResourceMetadata(void *pvOtaClient, char *resource_file_name, OtaFileMetadata_t **pResourceMetadata);
void  OtaClient_CloseServer(void *pvOtaClient);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __OTA_CLIENT_H__ */
