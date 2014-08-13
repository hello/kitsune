/*
 * OtaHttp.c - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
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
#ifndef __OTA_HTTP_H__
#define __OTA_HTTP_H__

#ifdef    __cplusplus
extern "C" {
#endif

#include "OtaClient.h"

#define HTTP_MAX_RETRIES          4000
#define HTTP_HEADER_STR           "HTTP/1.1 200"
#define HTTP_HEADER_SIZE          12     /* HTTP/x.x YYY */
#define HTTP_HEADER_OK            "200"  /* OK Code      */

#define SOCKET_SECURED            1
#define SOCKET_NON_SECURED        0
#define SOCKET_BLOCKING           0
#define SOCKET_NON_BLOCKING       1
#define SOCKET_PORT_DEFAULT       0
#define SOCKET_PORT_HTTP          80
#define SOCKET_PORT_HTTPS         443

/* service buffers */
#define HTTP_SEND_BUF_LEN   512  
#define HTTP_RECV_BUF_LEN  1440  
char *http_send_buf(void);
char *http_recv_buf(void);

/* HTTP */
_i32  http_extract_domain_by_url(char *url_name, char *domain_name, char *req_uri);
_i32  http_skip_headers(_i16 sockId);
_i32  http_build_request(char *pHttpReq, char *method, char *host_name, char *req_uri_prefix, char *req_uri, char *hdr_name, char *hdr_val);
_i16  http_connect_server(char *ServerName, _i32 IpAddr, _i32 port, _i32 secured, _i32 nonBlocking);
/* JSON */
_i32  json_parse_rsrc_metadata_url(char *media_response_buf, char *media_url);
_i32  json_parse_update_check_resp(_i16 sockId, RsrcData_t *pRsrcData, char *read_buf, _i32 size);

/* Services */
_i32  sl_Recv_eagain(_i16 sockId, void *pBuf, _i32 Len, _i32 flags, _i32 max_eagain);

/* Dropbox only */
_i32  json_parse_dropbox_metadata(_i16 sockId, RsrcData_t *pRsrcData, char *read_buf, _i32 size);
_i32  json_parse_dropbox_media_url(char *media_response_buf, char *media_url);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __OTA_HTTP_H__ */
