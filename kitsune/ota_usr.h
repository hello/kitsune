
#define MCU_VERSION    "MCU_01"
#ifdef _WIN32
#define VERSION_FROM_NWP_VER
#define OTA_VENDOR_STR "Vid01_Pid31_VerAA"  /* vendor id = 01, product id = 31, version will be filled online */
#else
#define OTA_VENDOR_STR "Vid01_Pid32_VerAA"  /* vendor id = 01, product id = 32, version will be filled online */
#endif

/* OTA server info */
/* --------------- */
#ifdef TI_OTA_SERVER
#define OTA_SERVER_NAME                 "10.0.0.13"
/*#define OTA_SERVER_NAME               "ild100069"" */
#define OTA_SERVER_IP_ADDRESS           0x0a00000d /*10.0.0.13 */
#define OTA_SERVER_SECURED              0
#define OTA_SERVER_REST_UPDATE_CHK      "/api/1/ota/update_check/vid:100" /* returns files/folder list */
#define OTA_SERVER_REST_RSRC_METADATA   "/api/1/ota/1001/metadata"        /* returns files/folder list */
#define OTA_SERVER_REST_HDR             " Connection: "
#define OTA_SERVER_REST_HDR_VAL         "keep-alive"
#else
#define OTA_SERVER_NAME                 "api.dropbox.com"
#define OTA_SERVER_IP_ADDRESS           0x00000000
#define OTA_SERVER_SECURED              1
#define OTA_SERVER_REST_UPDATE_CHK      "/1/metadata/auto/" /* returns files/folder list */
#define OTA_SERVER_REST_RSRC_METADATA   "/1/media/auto"     /* returns A url that serves the media directly */
#define OTA_SERVER_REST_HDR             "Authorization: Bearer "
#define OTA_SERVER_REST_HDR_VAL         "uSERguwUj98AAAAAAAAAdm8Vc_S0M0FIYYjg3Q0VYAZLQ9r58luEVq4LVYYosykm"
#define LOG_SERVER_NAME                 "api-content.dropbox.com"
#define OTA_SERVER_REST_FILES_PUT       "/1/files_put/auto/"
#endif

/* local data */
void *pvOtaApp;
OtaOptServerInfo_t g_otaOptServerInfo; /* must be global, will be pointed by extlib_ota */

void get_mac_address(_u8 *pMacAddress)
{
    _u8 macAddressLen = SL_MAC_ADDR_LEN;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen, pMacAddress);
}

void get_version_str(char *versionStr)
{
#ifdef VERSION_FROM_NWP_VER
    _u8  pConfigOpt;
    _u8  pConfigLen;
    SlVersionFull ver;

    pConfigOpt = SL_DEVICE_GENERAL_VERSION;
    pConfigLen = sizeof(ver);
    sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&pConfigOpt,&pConfigLen,(_u8  *)(&ver));

    /* use NWP service pack number */
    sprintf(versionStr, "%02d", ver.NwpVersion[3]);
#else
    /*#define MCU_VERSION    "MCU_15"     */
    versionStr[0] = MCU_VERSION[4];
    versionStr[1] = MCU_VERSION[5];
#endif
}

void print_version()
{
    _u8  pConfigOpt;
    _u8  pConfigLen;
    SlVersionFull ver;

    UARTprintf("----- MCU version: %s\n", MCU_VERSION);
    pConfigOpt = SL_DEVICE_GENERAL_VERSION;
    pConfigLen = sizeof(ver);
    sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&pConfigOpt,&pConfigLen,(_u8  *)(&ver));

    UARTprintf("CHIP %d\nMAC 31.%d.%d.%d.%d\nPHY %d.%d.%d.%d\nNWP %d.%d.%d.%d\nROM %d\nHOST %d.%d.%d.%d\n",
             ver.ChipFwAndPhyVersion.ChipId,
             ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
             ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
             ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
             ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3],
             ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
             ver.RomVersion,
             SL_MAJOR_VERSION_NUM,SL_MINOR_VERSION_NUM,SL_VERSION_NUM,SL_SUB_VERSION_NUM);
    sl_extLib_OtaGet(pvOtaApp, EXTLIB_OTA_GET_OPT_PRINT_STAT, NULL, NULL);
}

int proc_ota_init()
{
    UARTprintf("proc_ota: init\n");
    pvOtaApp = sl_extLib_OtaInit(RUN_MODE_NONE_OS | RUN_MODE_BLOCKING, NULL /* host storage callbacks, must be global */);
    if (pvOtaApp == NULL)
    {
        UARTprintf("proc_ota_init: ERROR from sl_extLib_OtaInit\n");
        return RUN_STAT_ERROR;
    }

    return RUN_STAT_OK;
}

int proc_ota_config(void)
{
    char vendorStr[20];

    /* set OTA server info */
    g_otaOptServerInfo.ip_address = OTA_SERVER_IP_ADDRESS;
    g_otaOptServerInfo.secured_connection = OTA_SERVER_SECURED;
    strcpy(g_otaOptServerInfo.server_domain, OTA_SERVER_NAME);
    strcpy(g_otaOptServerInfo.rest_update_chk, OTA_SERVER_REST_UPDATE_CHK);
    strcpy(g_otaOptServerInfo.rest_rsrc_metadata, OTA_SERVER_REST_RSRC_METADATA);
    strcpy(g_otaOptServerInfo.rest_hdr, OTA_SERVER_REST_HDR);
    strcpy(g_otaOptServerInfo.rest_hdr_val, OTA_SERVER_REST_HDR_VAL);
    strcpy(g_otaOptServerInfo.log_server_name, LOG_SERVER_NAME);
    strcpy(g_otaOptServerInfo.rest_files_put, OTA_SERVER_REST_FILES_PUT);
    get_mac_address(g_otaOptServerInfo.log_mac_address);
    sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_SERVER_INFO, sizeof(g_otaOptServerInfo), (char *)&g_otaOptServerInfo);

    /* set vendor ID */
    strcpy(vendorStr, OTA_VENDOR_STR);
    get_version_str(&vendorStr[15]);
    sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_VENDOR_ID, strlen(vendorStr), vendorStr);

    return RUN_STAT_OK;
}

int proc_ota_run_step()
{
    int status;
    int SetCommitInt = 1;
    char vendorStr[20];

    /*UARTprintf("proc_ota: call sl_extLib_OtaRun\n"); */
    status = sl_extLib_OtaRun(pvOtaApp);
    if (status < 0)
    {
        if (status == RUN_STAT_ERROR_CONTINUOUS_ACCESS_FAILURES)
        {
            /* ACCESS Error - should reset the NWP */
            UARTprintf("proc_ota_run_step: ACCESS ERROR from sl_extLib_OtaRun %d !!!!!!!!!!!!!!!!!!!!!!!!!!!\n", status);

            nwp_reset();
        }
        else
        {
            /* OTA will go back to IDLE and next sl_extLib_OtaRun will restart the process */
            UARTprintf("proc_ota_run_step: ERROR from sl_extLib_OtaRun %d\n", status);
        }
    }
    else if (status == RUN_STAT_NO_UPDATES)
    {
        /* OTA will go back to IDLE and next sl_extLib_OtaRun will restart the process */
        UARTprintf("proc_ota_run_step: status from sl_extLib_OtaRun: no updates, wait for next period\n");
    }
    else if (status == RUN_STAT_DOWNLOAD_DONE)
    {
        UARTprintf("proc_ota_run_step: status from sl_extLib_OtaRun: Download done, status = 0x%x\n", status);
        print_version();

        /* Check and test the new MCU/NWP image (if exists) */
        /* The image test, in case of new MCU image, will include instruction to the MCU bootloader to switch images in order to test the new image */
        /* The host should do the MCU reset in case of status from image test will be set */
        status = sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_TEST, 0, 0);
        if (status & OTA_ACTION_RESET_MCU)
        {
            /* reset the MCU in order to test the new image */
            UARTprintf("\nproc_ota_run_step: reset the MCU to test the new image ---\n");
            UARTprintf("proc_ota_run_step: Rebooting...\n\n");
            mcu_reset();
            /*
             * if we reach here, the platform does not support self reset
            */
            UARTprintf("proc_ota_run_step: return without MCU reset\n");
        }
        if (status & OTA_ACTION_RESET_NWP)
        {
            /* reset the NWP in order to test the new image */
            UARTprintf("\nproc_ota_run_step: reset the NWP to test the new image ---\n");
            nwp_reset();

            if( status < 0 )
            {
                SetCommitInt = 0;
                UARTprintf("proc_ota_run_step: ERROR from slAccess_start_and_connect!!!!!!!!!!!!!\n");
                /* Reset #2 - should in next versions to do NWP rollback */
                status = sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_COMMIT, sizeof(int), (char *)&SetCommitInt);
                if (status & OTA_ACTION_RESET_NWP)
                {
                    UARTprintf("\n!!!!!!!!!!!!!!!!!!!!!!proc_ota_run_step: reset #2 the NWP to test the new image ---\n");
                    nwp_reset();
                }
            }
        }
        /* Set OTA commited flag - this ends the current OTA process, will be rerstart on next period */
        SetCommitInt = 1;
        sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_COMMIT, sizeof(int), (char *)&SetCommitInt);
        /* set vendor ID - for next period check */
        strcpy(vendorStr, OTA_VENDOR_STR);
        get_version_str(&vendorStr[15]);
        sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_VENDOR_ID, strlen(vendorStr), vendorStr);
    }
    else
    {
        /*UARTprintf("proc_ota_run_step: status from sl_extLib_OtaRun %d, default continue\n", status); */
    }
    return 0;
}

int proc_ota_is_active()
{
    int ProcActive;
    int ProcActiveLen;

    /* check OTA process */
    sl_extLib_OtaGet(pvOtaApp, EXTLIB_OTA_GET_OPT_IS_ACTIVE, (signed long *)&ProcActiveLen, (char *)&ProcActive);

    return ProcActive;
}

#include "FreeRTOS.h"
#include "task.h"
void thread_ota(void * unused)
{
#define TEST_OTA 1
#if TEST_OTA
    int status, imageCommitFlag;

    /* Init OTA process */
    proc_ota_init();

    if (sl_mode != ROLE_STA) {
        /* If the MCU image is under test, the ImageCommit process will commit the new image and might reset the MCU */
        /* NWP commit process is not implemented yet */
        imageCommitFlag = OTA_ACTION_IMAGE_COMMITED;
        status = sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_COMMIT, sizeof(imageCommitFlag), (char *) &imageCommitFlag);
        if (status == OTA_ACTION_RESET_MCU) {
            UARTprintf("main: OTA_ACTION_RESET_MCU reset the platform to commit the new image\n");
            UARTprintf("main: Rebooting...\n\n");
            mcu_reset();
        }
    }

    /* Config OTA process - after net connection */
    proc_ota_config();
    print_version();

    /* noneOS process sharing loop */
    while(1)
    {

        if( sl_status & HAS_IP ) {
            /* Run OTA process */
            status = proc_ota_run_step();

            /* Check if to sleep or to continue serve the processes */
            if (!proc_ota_is_active())
            {
                UARTprintf("\r\nOTA task sleeping, MCU version=%s\r\n", MCU_VERSION);
                vTaskDelay(60*60*1000);
            }
        } else {
            UARTprintf("\r\nOTA task sleeping waiting for wifi, MCU version=%s\r\n", MCU_VERSION);
            vTaskDelay(60*60*1000);
        }
    }
#endif
}

