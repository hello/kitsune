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
 * ======== netwifi.h ========
 *
 * WiFi network functions
 *
 */

#define    NETWIFI_ERROR        (-1)
#define    NETWIFI_SUCCESS      0
#define    NETWIFI_RUNWAC       1
#define    NETWIFI_PROCEED      2


/*!
 *  ========== NetWiFi_pollWACFxn ==========
 *  Function type to be passed to NetWiFi_waitForNetwork
 *
 *  Passed to NetWiFi_waitForNetwork to be polled until a specified timeout
 *  expires. The application can use this to implement inform
 *  NetWiFi_waitForNetwork to run WAC.
 */
typedef int (NetWiFi_pollWACFxn)(void);

/*!
 *  @brief Wait for WiFi network to be setup
 *
 *  @param deviceName[in]       device name to be used as AP and MDNS name
 *  @param pollFxn              function polled to decide to run WAC or not
 *  @param timeout              poll timeout for pollFxn
 *
 *  @return IP Address packed in a long datatype
 */
long NetWiFi_waitForNetwork(char deviceName[],
                                     NetWiFi_pollWACFxn pollFxn,
                                     int timeout);
