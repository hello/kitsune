/*
 * ota_api.h - CC31xx/CC32xx Host Driver Implementation, external libs, extlib_ota
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
#ifndef __OTA_APP_EXT_H__
#define __OTA_APP_EXT_H__

void
UARTprintf(const char *pcString, ...);

#ifdef    __cplusplus
extern "C" {
#endif

#define OTA_LIB_VERSION    "1.00"
#define OTA_VENDOR_TEMPLATE_STR "Vid00_Pid00_Ver00"
#define OTASTAT_FILENAME "/sys/otastat.txt"

#define MAX_EAGAIN_RETRIES    10
#define MAX_RESOURCES         16
#define MAX_CONSECUTIVE_NET_ACCESS_ERRORS    10

#define MAX_SERVER_NAME     32
#define MAX_PATH_PREFIX     48
#define MAX_REST_HDRS_SIZE  96
#define MAX_USER_NAME_STR   8
#define MAX_USER_PASS_STR   8
#define MAX_CERT_STR        32
#define MAX_KEY_STR         32

typedef struct  
{
    _i32    ip_address; /* 0x0 – use server name */
    _i32    secured_connection;
    char   server_domain[MAX_SERVER_NAME];
    char   rest_update_chk[MAX_PATH_PREFIX];
    char   rest_rsrc_metadata[MAX_PATH_PREFIX];
    char   rest_hdr[MAX_REST_HDRS_SIZE];
    char   rest_hdr_val[MAX_REST_HDRS_SIZE];
    char   user_name[MAX_USER_NAME_STR];
    char   user_pass[MAX_USER_PASS_STR];
    char   ca_cert_filename[MAX_CERT_STR];
    char   client_cert_filename[MAX_CERT_STR];
    char   privatekey_filename[MAX_KEY_STR];
    /* logger */
    char   log_server_name[MAX_SERVER_NAME];
    char   rest_files_put[MAX_REST_HDRS_SIZE];
    _u8   log_mac_address[6];
} OtaOptServerInfo_t;

typedef _i32 (*OpenFileCB)  (_u8 *file_name, _i32 file_size, _u32 *ulToken, _i32 *lFileHandle, _i32 open_flags);
typedef _i32 (*WriteFileCB) (_i32 fileHandle, _i32 offset, char *buf, _i32 len);
typedef _i32 (*ReadFileCB)  (_i32 fileHandle, _i32 offset, char *buf, _i32 len);
typedef _i32 (*CloseFileCB) (_i32 fileHandle, _u8 *pCeritificateFileName,_u8 *pSignature ,_u32 SignatureLen);
typedef _i32 (*AbortFileCB)(_i32 fileHandle);
typedef _i32 (*CheckConvertFileCB)(_u8* file_name);

typedef struct 
{
    OpenFileCB   pOpenFile;
    WriteFileCB  pWriteFile;
    ReadFileCB   pReadFile;
    CloseFileCB  pCloseFile;
    AbortFileCB pAbortFile;
} FlcCb_t;

typedef struct 
{
    /* files server name */
    char cdn_url[256];

    /* file flags */
    _i32  flags;

    /* file name */
    char rsrc_file_name[256]; 
    char *p_file_name;
    _i32  sflash_file_size;

    /* certificate file name */
    char cert1_filename[64];
    char cert2_filename[64];
    char *p_cert_filename;

    /* signiture file name */
    char signature_filename[64];
    char *p_signature;
    char signature[256];
    _i32  signature_len;
} OtaFileMetadata_t;

typedef enum
{
    EXTLIB_OTA_SET_OPT_SERVER_INFO = 0,  /* see OtaOptServerInfo_t   */               
    EXTLIB_OTA_SET_OPT_VENDOR_ID,                    
    EXTLIB_OTA_SET_OPT_IMAGE_TEST,
    EXTLIB_OTA_SET_OPT_IMAGE_COMMIT
} OtaSetOpt_e;

typedef enum
{
    EXTLIB_OTA_GET_OPT_IS_ACTIVE,  
    EXTLIB_OTA_GET_OPT_IS_PENDING_COMMIT, 
    EXTLIB_OTA_GET_OPT_PRINT_STAT
} OtaGetOpt_e;

/* RunMode bitmap */
#define RUN_MODE_OS              (0 << 0)        /* bit 0: 1-NoneOs, 0-Os */
#define RUN_MODE_NONE_OS         (1 << 0)    
#define RUN_MODE_BLOCKING        (0 << 1)       /* bit 1: 1-NoneBlocking, 0-Blocking */
#define RUN_MODE_NONE_BLOCKING   (1 << 1)    

/* RunStatus */
#define RUN_STAT_DOWNLOAD_DONE                   0x0002
#define RUN_STAT_NO_UPDATES                      0x0001
#define RUN_STAT_OK                     0
#define RUN_STAT_CONTINUE               0
#define RUN_STAT_ERROR                          -1
#define RUN_STAT_ERROR_TIMEOUT                  -2
#define RUN_STAT_ERROR_CONNECT_OTA_SERVER       -3
#define RUN_STAT_ERROR_RESOURCE_LIST            -4
#define RUN_STAT_ERROR_METADATA                 -5
#define RUN_STAT_ERROR_CONNECT_CDN_SERVER       -6
#define RUN_STAT_ERROR_DOWNLOAD_SAVE            -7
#define RUN_STAT_ERROR_CONTINUOUS_ACCESS_FAILURES -1000

/* commit process decisions */
#define OTA_ACTION_RESET_MCU    0x1
#define OTA_ACTION_RESET_NWP    0x2

#define OTA_ACTION_IMAGE_COMMITED      1
#define OTA_ACTION_IMAGE_NOT_COMMITED  0

/*****************************************************************************

    API Prototypes

 *****************************************************************************/

/*!

    \addtogroup OtaApp
    @{

*/

/*!
    \brief Init OTA application

    Init the OTA application and modules 
    NOTE: runMode - only NONE_OS/BLOCKING is implemented

    \param[in] RunMode     bitmap Bit0-RUN_MODE_OS/NONEOS, Bit1-RUN_MODE_BLOCKING/NONE_BLOCKING
    \param[in] pFlcHostCb     list of callbacks to store files in external host storage instead of default NWP SFLASH storage

    \return                OTA control block pointer

    \sa                       
    \note                     
    \warning
    \par                 Example:  
    \code                
    For example: init OTA from host   
    
    pvOtaApp = sl_extLib_OtaInit(RUN_MODE_NONE_OS | RUN_MODE_BLOCKING, NULL);
    
    \endcode
*/
void *sl_extLib_OtaInit(_i32 runMode, FlcCb_t *pFlcHostCb);

/*!
    \brief Run OTA step

    Run one step from the OTA application state machine. Host should repeat calling this function and examine the return value 
    in order to check if OTA completed or got error or just need to be continued.
    This pattern is useful in host with noneOS. In host with OS, host should start OTA task and continuously calling this function till OTA completed
    
    \param[in] pvOtaApp     OTA control block pointer

    \return         On success, zero or positive bitmap numberis returned, On failure negative value
                    0 - RUN_STAT_CONTINUE - host should continue calling sl_extLib_OtaRun
                    1 - RUN_STAT_NO_UPDATES - no updates for now, host should try/call the OTA application in next period
                    2 - RUN_STAT_DOWNLOAD_DONE - Current OTA update completed, host should test bit 2,3 to reset the MCU/NWP
                    -1000 - RUN_STAT_ERROR_CONTINUOUS_ACCESS_FAILURES - fatal access error, OTA try more than 10 times to reach the OT Aserver and failed, it is recommended to reset the NWP
                    Other negative value - OTA error, OTA can recover from this failue, host should continue calling sl_extLib_OtaRun

    \sa                       
    \note                     
    \warning
    \par                 Example:  
    \code                
    For example: Run OTA from host   
    
    pvOtaApp = sl_extLib_OtaRun(pvOtaApp);
    
    \endcode
*/
_i32 sl_extLib_OtaRun(void *pvOtaApp);

/*!
    \brief Set OTA command/parameter

    Set OTA command/parameter 
    
    \param[in] pvOtaApp     OTA control block pointer
    \param[in] Option       Set option, could be one of the following: \n
                              EXTLIB_OTA_SET_OPT_SERVER_INFO - set server address and update_check and metadata rest command templates 
                              EXTLIB_OTA_SET_OPT_VENDOR_ID - example: "Vid01_Pid33_Ver18" identify the current application by vendor id, product id and version id
                              actually this is the directory to search the updates on the OTA server 
                              EXTLIB_OTA_SET_OPT_COMMITED - host commit the last OTA update, move to idle 

    \param[in] OptionLen    option structure length

    \param[in] pOptionVal   pointer to the option structure

    \return         On success, zero is returned. On error, -1 is returned

    \sa                       
    \note                     
    \warning
    \par                 Example:  
    \code                
    For example: Set OTA server info from host   
    
    sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_SERVER_INFO, sizeof(g_otaOptServerInfo), (char *)&g_otaOptServerInfo);
    
    \endcode
*/
_i32 sl_extLib_OtaSet(void *pvOtaApp, _i32 Option, _i32  OptionLen, char *pOptionVal);

/*!
    \brief Get OTA status

    Get OTA status 
    
    \param[in] pvOtaApp     OTA control block pointer
    \param[in] Option       Get option, could be one of the following: \n
                              EXTLIB_OTA_GET_OPT_IS_ACTIVE - return 1 if OTA is in process and not in idle
                              EXTLIB_OTA_GET_OPT_PRINT_STAT - print statistics

    \param[in] OptionLen    option structure length

    \param[in] pOptionVal   pointer to the option structure

    \return         On success, zero is returned. On error, -1 is returned

    \sa                       
    \note                     
    \warning
    \par                 Example:  
    \code                
    For example: Get OTA running status   
    
    sl_extLib_OtaGet(pvOtaApp, EXTLIB_OTA_GET_OPT_IS_ACTIVE, &ProcActiveLen, (char *)&ProcActive);
    
    \endcode
*/
_i32 sl_extLib_OtaGet(void *pvOtaApp, _i32 Option, _i32 *OptionLen, char *pOptionVal);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /* __OTA_APP_EXT_H__ */
