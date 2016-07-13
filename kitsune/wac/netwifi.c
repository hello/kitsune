/*
 * Copyright (c) 2014-2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * ======== netwifi.c ========
 *
 * WiFi network functions
 *
 */
#include <stdbool.h>
#include <stdint.h>

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/knl/Task.h>

#include <simplelink.h>
#include <wlan.h>
#include <osi.h>
#include <waclib/waclib.h>

#include "netwifi.h"

#define MAX_NUM_PROFILES        7
#define SL_STOP_TIMEOUT         200
#define POLLINTERVAL            100
#define MFI_I2C_ADDRESS         0x10

static uint32_t deviceConnected = false;
/*static*/ uint32_t g_ipv4_acquired = false;
/*static*/ uint32_t g_ipv6_acquired = false;

/*
 *  ======== SimpleLinkSockEventHandler ========
 */
void SimpleLinkSockEventHandler(SlSockEvent_t *sockEvent) {}

void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *sockEvent) {}
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *pSlFatalErrorEvent) {}
_u32 TimerGetCurrentTimestamp()
{
    /* TODO */
    return (0);
}


/*
 *  ======== SimpleLinkWlanEventHandler ========
 */
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pArgs) {
    switch (pArgs->Id) {
        case SL_WLAN_EVENT_CONNECT:
            deviceConnected = true;
            break;

        case SL_WLAN_EVENT_DISCONNECT:
            deviceConnected = false;
            break;
    }
}

/*
 *  ======== SimpleLinkNetAppEventHandler ========
 */
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pArgs) {
    switch (pArgs->Id) {
        case SL_NETAPP_EVENT_IPV4_ACQUIRED:
            g_ipv4_acquired = true;
            break;

        case SL_NETAPP_EVENT_IPV6_ACQUIRED:
            g_ipv6_acquired = true;
            break;

        case SL_NETAPP_EVENT_DHCPV4_LEASED:
            break;

        case SL_NETAPP_EVENT_DHCPV4_RELEASED:
            break;
    }
}

/*
 *  ======== SimpleLinkHttpServerCallback ========
 */
void SimpleLinkHttpServerCallback(SlNetAppHttpServerEvent_t *pHttpServerEvent,
                                  SlNetAppHttpServerResponse_t *pHttpServerResponse) {}

/*
 *  ======== NetWiFi_runWAC ========
 *  Run Wireless Accessory Configuration (WAC)
 */
int NetWiFi_runWAC(char deviceName[])
{
    int wacFlags = SL_WAC_HOMEKIT;
    char wacManufacturerName[] = "TI";
    char wacModel[] = "SLink1,1";
    int simplelinkRole;
    unsigned char* deviceURN = (unsigned char *)deviceName;
    int deviceURNLen = strlen(deviceName);
    int ret;
    bool retry = false;

    Log_print0(Diags_ENTRY, "NetWifi run WAC Start");

    do {
        /* Set to AP mode, set device URN and restart network processor */
        sl_WlanSet(SL_WLAN_CFG_AP_ID, SL_WLAN_AP_OPT_SSID, deviceURNLen,
                deviceURN);
        sl_NetAppSet(SL_NETAPP_DEVICE_ID, SL_NETAPP_DEVICE_URN, deviceURNLen,
                deviceURN);
        sl_WlanSetMode(ROLE_AP);
        sl_Stop(SL_STOP_TIMEOUT);
        simplelinkRole =  sl_Start(NULL,NULL,NULL);

        Assert_isTrue(simplelinkRole == ROLE_AP, NULL);

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
                Log_error1("WacDeInit returned error code %d", ret);
            }
        }

        ret = sl_ExtLib_WacInit(MFI_I2C_ADDRESS, NULL);
        if (ret < 0) {
            Log_error1("WacInit returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacSet(SL_WAC_FLAGS, (char*)&wacFlags, sizeof(wacFlags));
        if (ret < 0){
            Log_error1("WacSet returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacSet(SL_WAC_VENDOR, (char*)&wacManufacturerName,
                                sizeof(wacManufacturerName));
        if (ret < 0) {
            Log_error1("WacSet returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacSet(SL_WAC_MODEL, (char*)&wacModel, sizeof(wacModel));
        if (ret < 0) {
            Log_error1("WacSet returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacRun(0);
        if (ret < 0) {
            Log_error1("WacRun returned error code %d", ret);
            retry = true;
            continue;
        }

        ret = sl_ExtLib_WacDeInit();
        if (ret < 0) {
            Log_error1("WacDeInit returned error code %d", ret);
        }

        retry = false;

    } while(retry);

    return NETWIFI_SUCCESS;
}

/*
 *  ======== NetWiFi_waitForNetwork ========
 *  Wait for Network initialization to complete
 */
long NetWiFi_waitForNetwork(char deviceName[],
                                     NetWiFi_pollWACFxn pollFxn,
                                     int timeout)
{
    SlNetCfgIpV4Args_t ipV4;
    _u16        len = sizeof(ipV4);
    _u16           DhcpBits;
    _u8 macAddr[6];
    SlWlanSecParams_t secParams;
    SlWlanGetSecParamsExt_t secExtParams;
    _i32 slRetVal;
    SlFsRetToFactoryCommand_t RetToFactoryCommand;
    _u32 apPriority;
    _i16 index = 0;
    bool profileFound = false;
    int simplelinkRole;
    _i8 apName[32];
    _i16 apNameLen = sizeof(apName);
    _u16 ConfigOpt = 0;   //return value could be one of the following: SL_NETCFG_ADDR_DHCP / SL_NETCFG_ADDR_DHCP_LLA / SL_NETCFG_ADDR_STATIC
    _i32 status;
    SlNetCfgIpV6Args_t ipV6 = {0};
    _u8 needToReset = 0;
    _u32 IfBitmap = 0;
    int ret;

    simplelinkRole = sl_Start(NULL, NULL, NULL);

    /*
     *  Run poll fxn every 0.1s until timeout is reached
     *  If pollFxn returns WAIT_FOR_NETWORK_RUN_WAC, then
     *  switch to AP mode to allow WAC to run in switch
     *  statement below
     */
    if (pollFxn) {
        while (timeout > 0) {
            if (pollFxn() == NETWIFI_RUNWAC) {

                RetToFactoryCommand.Operation = SL_FS_FACTORY_RET_TO_DEFAULT;
                slRetVal = sl_FsCtl( (SlFsCtl_e)SL_FS_CTL_RET_TO_FACTORY, 0, NULL ,
                                    (_u8 *)&RetToFactoryCommand , sizeof(SlFsRetToFactoryCommand_t), NULL, 0 , NULL );
                if(slRetVal < 0) return -1;

                sl_Stop(SL_STOP_TIMEOUT);
                simplelinkRole =  sl_Start(NULL,NULL,NULL);
                break;
            }
            Task_sleep(POLLINTERVAL);
            timeout = timeout - POLLINTERVAL;
        }
    }

    switch (simplelinkRole) {
        case ROLE_AP:
            ret = NetWiFi_runWAC(deviceName);
            Assert_isTrue(ret == NETWIFI_SUCCESS, NULL);
            break;

        case ROLE_STA:
            /* check IPV4 station mode */
            len = sizeof(SlNetCfgIpV4Args_t);
            status = sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE, &ConfigOpt, &len,
                    (_u8 *)&ipV6);
            Assert_isTrue(status == 0, NULL);
            (void)status;

            if (SL_NETCFG_ADDR_DHCP_LLA != ConfigOpt) {
        needToReset = 1;
        ConfigOpt = SL_NETCFG_ADDR_DHCP_LLA;
        status = sl_NetCfgSet(SL_NETCFG_IPV4_STA_ADDR_MODE, ConfigOpt, NULL,
                        NULL);
                Assert_isTrue(status == 0, NULL);
            }

            /* IPV6 local stateless */
            IfBitmap = 0;
            len = sizeof(IfBitmap);
            /* check and enable ipv6 local */
            status = sl_NetCfgGet(SL_NETCFG_IF, NULL, &len, (_u8 *)&IfBitmap);
            Assert_isTrue(status == 0, NULL);

            if (SL_NETCFG_IF_IPV6_STA_LOCAL != (IfBitmap & SL_NETCFG_IF_IPV6_STA_LOCAL)) {
        /* need to enable ipv6 local */
        needToReset = 1;
                IfBitmap = SL_NETCFG_IF_IPV6_STA_LOCAL;
                status = sl_NetCfgSet(SL_NETCFG_IF, SL_NETCFG_IF_STATE, sizeof(IfBitmap),
                        (_u8 *)&IfBitmap);
                Assert_isTrue(status == 0, NULL);
            }
            // check and set ipv6 statless

            len = sizeof(SlNetCfgIpV6Args_t);
            ConfigOpt = 0;
            status = sl_NetCfgGet(SL_NETCFG_IPV6_ADDR_LOCAL, &ConfigOpt, &len,
                    (_u8 *)&ipV6);
            Assert_isTrue(status == 0, NULL);

            if (SL_NETCFG_ADDR_STATELESS != (ConfigOpt & SL_NETCFG_ADDR_STATELESS)) {
        status = sl_NetCfgSet(SL_NETCFG_IPV6_ADDR_LOCAL,
                        SL_NETCFG_ADDR_STATELESS, NULL, NULL);
                Assert_isTrue(status == 0, NULL);
            }

            if (needToReset)
            {
        sl_Stop(0);

                deviceConnected = false;
                g_ipv4_acquired = false;
                g_ipv6_acquired = false;

        sl_Start(NULL, NULL, NULL);
            }

            /*
             *  If the device has a stored profile wait till it connects to it.
             *  If no profile is stored on the device run WAC.
             */
            while (profileFound == false && index < MAX_NUM_PROFILES) {
                if ((sl_WlanProfileGet(index, apName, &apNameLen, macAddr, &secParams,
                            &secExtParams, &apPriority)) >= 0) {
                    profileFound = true;
                }
                index++;
            }

            if (profileFound == true) {
                while ((deviceConnected != true) || (g_ipv4_acquired != true) ||
                        (g_ipv6_acquired != true)) {
                    Task_sleep(50);
                }
            }
            else {
                ret = NetWiFi_runWAC(deviceName);
                Assert_isTrue(ret == NETWIFI_SUCCESS, NULL);
            }
            break;

        default:
            ret = NetWiFi_runWAC(deviceName);
            Assert_isTrue(ret == NETWIFI_SUCCESS, NULL);
            break;
    }

    /* Retrieve & print the IP address */
    sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE, &DhcpBits, &len,
            (unsigned char *)&ipV4);

    return (ipV4.Ip);
}
