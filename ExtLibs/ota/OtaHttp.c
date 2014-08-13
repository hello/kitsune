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
#include "simplelink.h"
#include "OtaCommon.h"
#include "ota_api.h"
#include "OtaClient.h"
#include "OtaHttp.h"

static char send_buf[HTTP_SEND_BUF_LEN];       
static char recv_buf[HTTP_RECV_BUF_LEN];

char *http_send_buf(void) { return send_buf; }
char *http_recv_buf(void) { return recv_buf; }

_i32 g_ConsecutiveNetAccessNetErrors = 0;

/* HTTP Get stcuture:
    GET <URI> HTTP/1.1
    Host: <Domain>
    Authorization: Bearer <Access Token>
*/
_i32 http_build_request(char *pHttpReq, char *method, char *host_name, char *req_uri_prefix, char *req_uri, char *hdr_name, char *hdr_val)
{
    /* start with method GET/POST/PUT */
    strcpy(pHttpReq, method);

    /* fill uri_req_prefix, metadata/media */
    if (req_uri_prefix && strlen(req_uri_prefix))
    {
        strcat(pHttpReq, req_uri_prefix);
    }
    /* fill request URI */
    if (req_uri && strlen(req_uri))
    {
        strcat(pHttpReq, req_uri);
    }

    /* fill domain */
    strcat(pHttpReq, " HTTP/1.1\r\nhost: ");
    strcat(pHttpReq, host_name);
    strcat(pHttpReq, "\r\n");

    /* fill access_token */
    if (hdr_name && strlen(hdr_name))
    {
        strcat(pHttpReq, hdr_name);
        strcat(pHttpReq, hdr_val);
        strcat(pHttpReq, "\r\n");
    }

    strcat(pHttpReq, "\r\n");

    return OTA_STATUS_OK;
}

/* 
    parse url into domain name and uri_name    
    https://dl.cloud_storage_provider_1.com/1/view/3ntw3xgkgselev9/Apps/SL_OTA/IlanbSmallFile.txt
*/
_i32 http_extract_domain_by_url(char *url_name, char *domain_name, char *req_uri)
{
    char *pBuf;
    _i32 len;

    if (url_name == NULL)
    {
        Report("http_extract_domain_by_url: Error url name\n");
        return OTA_STATUS_ERROR;
    }

    /* extract domanin name, extract only the domain name between // to the first / */
    /* example: https://dl.cloud_storage_provider_1.com/1/view/3ntw3xgkgselev9/Apps/SL_OTA/IlanbSmallFile.txt */
    pBuf = url_name;
    pBuf = strstr(pBuf, "//");
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    pBuf += 2;
    len = strstr(pBuf, "/") - pBuf; /* ends with / */
    if (len <= 0)
        return OTA_STATUS_ERROR;
    strncpy (domain_name, pBuf, len);
    domain_name[len] = 0;
    pBuf += len;

    /* ToDo add port number extraction  */

    /* extract domanin name */
    strcpy(req_uri, pBuf);

    return OTA_STATUS_OK;
}

_i32 sl_Recv_eagain(_i16 sockId, void *pBuf, _i32 Len, _i32 flags, _i32 max_eagain)
{
    _i32 len;

    do
    {
        len = sl_Recv(sockId, pBuf, (_u16)Len, (_i16)flags);
        if (len != SL_EAGAIN) /*Try Again */
            break;
    } while(--max_eagain);

    return len;
}

_i32 http_skip_headers(_i16 sockId)
{  
    _i32 len;  
    char *pHeader_string;
    char header_string[HTTP_HEADER_SIZE+1];
    _i32 retry=0;

    pHeader_string = &header_string[0];

    /* First thing - look for success code 200 */
    len = sl_Recv_eagain(sockId, pHeader_string, HTTP_HEADER_SIZE, 0, MAX_EAGAIN_RETRIES); /*Get beginning of server response http header */
    if (len < 0)
    {
        Report("http_skip_headers: ERROR sl_Recv_eagain, status=%d\n", len);
        return OTA_STATUS_ERROR;
    }

    /* Second chance */
    if (len != HTTP_HEADER_SIZE)
    {
        /* second chance */
        len += sl_Recv_eagain(sockId, &pHeader_string[len], HTTP_HEADER_SIZE-len, 0, MAX_EAGAIN_RETRIES); /*Get beginning of server response http header */
        if (len < 0)
        {
            Report("http_skip_headers: ERROR sl_Recv_eagain, status=%d\n", len);
            return OTA_STATUS_ERROR;
        }
        if (len != HTTP_HEADER_SIZE)
        {
            Report("http_skip_headers: fail on retry #2, len=%d\n", len);
            return OTA_STATUS_ERROR;
        }
    }

    if (strncmp (HTTP_HEADER_OK, &pHeader_string[9], 3)) /* search for HTTP/x.x 200 */
    {
        pHeader_string[HTTP_HEADER_SIZE]=0; /* just for printing */
        Report("http_skip_headers: http error code %s\n", pHeader_string);
        return OTA_STATUS_ERROR;
    }

    /* look for end of response header*/
    while (1)
    {
        len = sl_Recv_eagain(sockId, &header_string[0], 2, 0, MAX_EAGAIN_RETRIES); 
        if (len < 0)
        {
            Report("http_skip_headers: ERROR sl_Recv_eagain, status=%d\n", len);
            return OTA_STATUS_ERROR;
        }
        if (!(strncmp (header_string, "\r\n", 2))) /*if an entire carriage return is found, search for another */
        {
            sl_Recv_eagain(sockId, &header_string[0], 2, 0, MAX_EAGAIN_RETRIES); 
            if (!(strncmp (header_string, "\r\n", 2)))
            {
                return OTA_STATUS_OK;
            }
        }
        else if (!(strncmp (header_string, "\n\r", 2))) /*if two carriage returns are found in an offset */
        {
            sl_Recv_eagain(sockId, &header_string[0], 1, 0, MAX_EAGAIN_RETRIES); /*grab another byte to negate the offset */
            if (!(strncmp (header_string, "\n", 1)))
            {
                return OTA_STATUS_OK;
            }
        }
        if (retry++ > HTTP_MAX_RETRIES) 
        {
            Report("http_skip_headers: search end of headeer mac retries\n");
            return OTA_STATUS_ERROR;
        }
    }  
}

/* 
    example media url
    {
        "url": "https://cloud_storage_provider_1/1/view/3ntw3xgkgselev9/Apps/SL_OTA/IlanbSmallFile.txt",
        "expires": "Wed, 07 May 2014 15:54:25 +0000"
    }
*/
_i32 json_parse_dropbox_media_url(char *media_response_buf, char *media_url)
{
    char *pBuf = media_response_buf;
    _i32 len;

    pBuf = strstr(pBuf, "\r\n\r\n");  /* skip HTTP response header */
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    pBuf = strstr(pBuf, "url"); 
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    pBuf += 7;
    len = strstr(pBuf, "\", ") - pBuf; /* ends with ", */
    if (len <= 0)
        return OTA_STATUS_ERROR;

    strncpy (media_url, pBuf, len);
    media_url[len] = 0;

    return OTA_STATUS_OK;
}

/* example for metadata file:         
        {
            "revision": 6,
            "rev": "623446f31",
            "thumb_exists": false,
            "bytes": 244048,
            "modified": "Sun, 04 May 2014 08:17:44 +0000",
            "client_mtime": "Sun, 04 May 2014 08:17:44 +0000",
            "path": "/Mac31041_Phy1533.ucf",
            "is_dir": false,
            "icon": "page_white",
            "root": "root_dir",
            "mime_type": "application/octet-stream",
            "size": "238.3 KB"
        },
*/
_i32 json_parse_dropbox_metadata(_i16 sockId, RsrcData_t *pRsrcData, char *read_buf, _i32 size)
{
    RsrcData_t currentFile;
    char *pBuf;
    char *pRecord;
    _i32 len;
    char fileNumber=0;
    char size_str[64];

    pBuf = read_buf;
    pBuf = strstr(pBuf, "["); /* start of directory files */
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    while (1)
    {
        pBuf = strstr(pBuf, "{"); /* start of next file */
        if (pBuf == NULL)
            return OTA_STATUS_ERROR;
        /* read more if no end of file record - between { } */
        if (strstr(pBuf, "}") <= pBuf) /* file recored end not found */
        {
            _i32 left_len = read_buf+size-pBuf;
            if (left_len < 0)
            {
                /* ToDo: need to check it */
                return OTA_STATUS_ERROR;
            }
            strncpy(read_buf, pBuf, left_len); /* copy current file to start of read buffer */
            memset(&read_buf[left_len], 0, HTTP_RECV_BUF_LEN-left_len); /* copy current file to start of read buffer */
            size = sl_Recv_eagain(sockId, &read_buf[left_len], HTTP_RECV_BUF_LEN-left_len, 0, MAX_EAGAIN_RETRIES); /*Get media link */
            size += left_len;
            pBuf = read_buf;
            if (strstr(read_buf, "}") <= pBuf) /* still not found, exit */
            {
                return OTA_STATUS_OK;
            } 
        }
        /* at this point we have all next file record */

        pRecord = pBuf;

        /* extract file size */
        pBuf = strstr(pRecord, "bytes"); /* start of directory files */
        if (pBuf == NULL)
            return OTA_STATUS_ERROR;

        pBuf += 8;
        len = strstr(pBuf, ",") - pBuf; /* end of size */
        if (len <= 0)
            return OTA_STATUS_ERROR;

        strncpy (size_str, pBuf, len);
        size_str[len] = 0;
        pBuf += len;

        /* extract file path */
        pBuf = strstr(pRecord, "path"); /* start of directory files */
        if (pBuf == NULL)
            return OTA_STATUS_ERROR;

        pBuf += 8;
        len = strstr(pBuf, "\",") - pBuf; /* end of size */
        if (len <= 0)
            return OTA_STATUS_ERROR;

        strncpy (currentFile.filename, pBuf, len);
        currentFile.filename[len] = 0;
        pBuf += len;
        
        pBuf = 1+strstr(pBuf, "}"); /* go to end of resource block */

        currentFile.size = atoi(size_str);
        pRsrcData[fileNumber++] = currentFile;
        Report("    metadata file=%s, size=%d\n", currentFile.filename, currentFile.size);

        /* end of file - last file doesn't have a , */
        if (pBuf[0] != ',')
            break; 
    }

    /* The receive buffer should have the - "}" pattern, if not */
    /* we are yet to receive the last packet */
    if( strstr(pBuf, "}") == NULL )
    {
        /* Just receive this packet and discard it */
        sl_Recv_eagain(sockId, &read_buf[0], HTTP_RECV_BUF_LEN, 0,
                       MAX_EAGAIN_RETRIES);
    }

    return fileNumber;  
}


/* example for metadata file:         
{
    "resource_type_list":[
    {
        "resource_type": "RSRC_TYPE_FILE",
        "num_resources": 2,
        "resource_list":[           
            {
                "resource_id":  "/Mac31041_Phy1533.ucf",
                "size":  244048 
            }, 
            {
                "resource_id":  "/IlanbSmallFile.txt",
                "size":  4000 
            }
        ],
        }
    ],
}
*/

_i32 json_parse_update_check_resp(_i16 sockId, RsrcData_t *pRsrcData, char *read_buf, _i32 size)
{
    RsrcData_t rsrcData;
    char size_str[64];
    char *pBuf;
    _i32 len;
    char rsrcNum=0;

    pBuf = read_buf;
    pBuf = strstr(pBuf, "["); /* start of directory files */
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    /* Skip to second list - ToDo parse full 2 bundle lists */
    pBuf = strstr(pBuf, "["); /* start of directory files */
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    while (1)
    {
        pBuf = strstr(pBuf, "{"); /* start of next file */
        if (pBuf == NULL)
            return OTA_STATUS_ERROR;
        /* read more if no end of file record - between { } */
        if (strstr(pBuf, "}") <= pBuf) /* file recored end not found */
        {
            _i32 left_len = read_buf+size-pBuf;
            strncpy(read_buf, pBuf, left_len); /* copy current file to start of read buffer */
            size = sl_Recv_eagain(sockId, &read_buf[left_len], HTTP_RECV_BUF_LEN-left_len, 0, MAX_EAGAIN_RETRIES); /*Get media link */
            pBuf = read_buf;
            if (strstr(pBuf, "}") <= pBuf) /* still not found, exit */
            {
                return OTA_STATUS_OK;
            } 
        }
        /* at this po_i32 we have all next file record */

        /* Todo Add check response after each strstr!!!!! */

        /* extract file size */
        pBuf = strstr(pBuf, "size"); /* "size": 1000, */
        if (pBuf == NULL)
            return OTA_STATUS_ERROR;

        pBuf += sizeof("size")+3;
        len = strstr(pBuf, ",") - pBuf; 
        if (len <= 0)
            return OTA_STATUS_ERROR;
        strncpy (size_str, pBuf, len);
        size_str[len] = 0;
        pBuf += len;

        /* extract resource_id */
        pBuf = strstr(pBuf, "resource_id");  /* "resource_id": "<filename>", */
        if (pBuf == NULL)
            return OTA_STATUS_ERROR;

        pBuf += sizeof("resource_id")+3;
        len = strstr(pBuf, "\"") - pBuf;
        if (len <= 0)
            return OTA_STATUS_ERROR;
        strncpy (rsrcData.filename, pBuf, len);
        rsrcData.filename[len] = 0;
        pBuf += len;

        
        /* skip to end of list */
        pBuf = 1+strstr(pBuf, "}");

        /* copy resource data to the list */
        rsrcData.size = atoi(size_str);
        pRsrcData[rsrcNum++] = rsrcData;
        Report("resource file=%s, size=%d\n", rsrcData.filename, rsrcData.size);

        /* end of file - last file doesn't have a , */
        if (pBuf[0] != ',')
            break; 
    }
    return rsrcNum;  
}

/* 
    example media url
    {
        "resource_type": "RSRC_TYPE_FILE",
        "name": "/Mac31041_Phy1533",
        "CDN_url": "https://<server_name>/api/1/ota/local_cdn/SL_OTA/Mac31041_Phy1533.ucf",
        "bytes": 244048,
    }
*/
_i32 json_parse_rsrc_metadata_url(char *media_response_buf, char *media_url)
{
    char *pBuf = media_response_buf;
    _i32 len;

    pBuf = strstr(pBuf, "\r\n\r\n");  /* skip HTTP response header */
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    pBuf = strstr(pBuf, "CDN_url"); 
    if (pBuf == NULL)
        return OTA_STATUS_ERROR;

    pBuf += 7;
    len = strstr(pBuf, "\", ") - pBuf; /* ends with ", */
    if (len <= 0)
        return OTA_STATUS_ERROR;
    strncpy (media_url, pBuf, len);
    media_url[len] = 0;

    return OTA_STATUS_OK;
}


/* http_connect_server: create TCP socket to the server */
/* ----------------------------------------------------- */
_i16 http_connect_server(char *ServerName, _i32 IpAddr, _i32 port, _i32 secured, _i32 nonBlocking)
{
    _i32 Status;
    SlSockAddrIn_t  Addr;
    _i32 AddrSize;
    _i16 SockID = 0;
    _u32 ServerIP;
    SlTimeval_t tTimeout;

    if (IpAddr == 0)
    {
        /* get host IP by name */
        Status = sl_NetAppDnsGetHostByName((_i8 *)ServerName, strlen(ServerName), &ServerIP, SL_AF_INET);
        if(0 > Status )
        {
            Report("http_connect_server: ERROR sl_NetAppDnsGetHostByName, status=%d\n", Status);
            if (g_ConsecutiveNetAccessNetErrors++ >= MAX_CONSECUTIVE_NET_ACCESS_ERRORS)
            {
                g_ConsecutiveNetAccessNetErrors = 0;
                return OTA_STATUS_ERROR_CONTINUOUS_ACCESS;
            }
            return OTA_STATUS_ERROR;
        }
    }
    else
    {
        ServerIP = IpAddr;
    }

    g_ConsecutiveNetAccessNetErrors = 0;

    if (port == SOCKET_PORT_DEFAULT)
    {
        port = secured ? SOCKET_PORT_HTTPS : SOCKET_PORT_HTTP;
    }

    /* create socket */
    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(((_u16)port));
    Addr.sin_addr.s_addr = sl_Htonl(ServerIP);

    AddrSize = sizeof(SlSockAddrIn_t);

    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, (secured? SL_SEC_SOCKET : 0));
    if( SockID < 0 )
    {
        Report("http_connect_server: ERROR Socket Open, status=%d\n", SockID);
        return OTA_STATUS_ERROR;
    }

    /* set recieve timeout */
    tTimeout.tv_sec=2;
    tTimeout.tv_usec=0;
    Status = sl_SetSockOpt(SockID, SL_SOL_SOCKET, SL_SO_RCVTIMEO, &tTimeout, sizeof(SlTimeval_t));
    if( Status < 0 )
    {
        sl_Close(SockID);
        Report("http_connect_server: ERROR sl_SetSockOpt, status=%d\n", SockID);
        return OTA_STATUS_ERROR;
    }

    /* connect to the server */
    Status = sl_Connect(SockID, ( SlSockAddr_t *)&Addr, (_u16)AddrSize);
    if(( Status < 0 ) && (Status != (-453))) /*-453 - connected secure socket without server authentication */
    {
        sl_Close(SockID);
        Report("http_connect_server: ERROR Socket Connect, status=%d\n", Status);
        return OTA_STATUS_ERROR;
    }

    /* handle non blocking */
    if (nonBlocking)
    {
        sl_SetSockOpt(SockID, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &nonBlocking, sizeof(nonBlocking));
    }

    return SockID;
}

