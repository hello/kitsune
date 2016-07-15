
#include "waclib/waclib.h"

#include "wlan.h"
#include "stdbool.h"

#include "homekit.h"
#include "kit_assert.h"
#include "uart_logger.h"

#define NAME "sense"

#define SL_STOP_TIMEOUT 200

//could be 0x11 too
#define MFI_I2C_ADDRESS 0x10

void sl_ExtLib_Time_Delay(unsigned long ulDelay) {
	vTaskDelay(ulDelay);
}

int Cmd_wac(int argc, char *argv[]) {
    int wacFlags = SL_WAC_HOMEKIT;
    char wacManufacturerName[] = "Hello Inc";
    char wacModel[] = "SLink1,1";
    int simplelinkRole;
    unsigned char* deviceURN = (unsigned char *)NAME;
    int deviceURNLen = strlen(NAME);
    int ret;
    bool retry = false;

    LOGI("NetWifi run WAC Start");

    do {
        /* Set to AP mode, set device URN and restart network processor */
        sl_WlanSet(SL_WLAN_CFG_AP_ID, SL_WLAN_AP_OPT_SSID, deviceURNLen,
                deviceURN);
        sl_NetAppSet(SL_NETAPP_DEVICE_ID, SL_NETAPP_DEVICE_URN, deviceURNLen,
                deviceURN);
        sl_WlanSetMode(ROLE_AP);
        sl_Stop(SL_STOP_TIMEOUT);
        simplelinkRole =  sl_Start(NULL,NULL,NULL);

        assert(simplelinkRole == ROLE_AP);

        /*
         * IMPORTANT - This unregister all services is here only for the case
         * where a WAC session did not end and one of the services remained.
         * This should not occur in real scenarios, as WAC occurs only once, and
         * is expected to end successfully. It should be the user's
         * responsibility to ensure no WAC MDNS services are running prior to
         * beginning a new WAC procedure.
         */
        sl_NetAppMDNSUnRegisterService((const signed char *)" ", 0, 0);

        if (retry) {
            ret = sl_ExtLib_WacDeInit();
            if (ret < 0) {
                LOGE("WacDeInit returned error code %d", ret);
            }
        }

        ret = sl_ExtLib_WacInit(MFI_I2C_ADDRESS, NULL);
        if (ret < 0) {
            LOGE("WacInit returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacSet(SL_WAC_FLAGS, (char*)&wacFlags, sizeof(wacFlags));
        if (ret < 0){
            LOGE("WacSet returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacSet(SL_WAC_FLAGS, (char*)&wacManufacturerName,
                                sizeof(wacManufacturerName));
        if (ret < 0) {
            LOGE("WacSet returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacSet(SL_WAC_MODEL, (char*)&wacModel, sizeof(wacModel));
        if (ret < 0) {
            LOGE("WacSet returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacRun(0);
        if (ret < 0) {
            LOGE("WacRun returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacDeInit();
        if (ret < 0) {
            LOGE("WacDeInit returned error code %d", ret);
        }

        retry = false;

    } while(retry);

    return 0;
}
