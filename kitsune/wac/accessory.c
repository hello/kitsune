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
 *  ======== accessory.c ========
 */

#include <stdint.h>
#include <stdbool.h>

#include <ti/hap/HAPAcc.h>
#include <ti/sysbios/knl/Task.h>

#include "led.h"

static HAPEngine_SrvStatus blink_identifyFxn(unsigned int aId);

/* TODO: Following HAPEngine constants should come from HAPEngine.h */
#define HAPEngine_RAWSETUPCODE        (1)

HAPEngine_ServiceDesc *accessory_serviceArray[] = {
    (HAPEngine_ServiceDesc *)&ledServiceDesc
};

/* Note that service id and characterstic id is determined by the index
 * in the respective arrays */
HAPEngine_AccessoryDesc accessory_services = {
    NULL, NULL, NULL, NULL, /* initialized at runtime */
    &blink_identifyFxn,
    1,
    sizeof(accessory_serviceArray)/sizeof(accessory_serviceArray[0]),
    &accessory_serviceArray
};

/* Utility fxn to initialize any accessory values at runtime */
int accessory_init(HAPEngine_AccessoryDesc *a)
{
    a->name = "Blink";
    a->manufacturer = "Texas Instruments";
    a->model = "ABCDE";
    a->serialNumber = "12345";

  //  led_init();

    return (0);
}

static HAPEngine_SrvStatus blink_identifyFxn(unsigned int aId)
{
    int i;

    for (i = 0; i < 5; i++) {
        ledIdentifyRoutine();
        Task_sleep(500);
    }
    ledRestoreState();

    return (HAPEngine_SrvStatus_EOK);
}

extern int blink_getSetupCodeFxn(char *setupCode, uint8_t *salt,
        uint8_t *verifier)
{
    setupCode[0]  = '1';
    setupCode[1]  = '1';
    setupCode[2]  = '1';
    setupCode[3]  = '-';
    setupCode[4]  = '2';
    setupCode[5]  = '2';
    setupCode[6]  = '-';
    setupCode[7]  = '3';
    setupCode[8]  = '3';
    setupCode[9] = '3';
    setupCode[10] = '\0';

    return (HAPEngine_RAWSETUPCODE);
}
