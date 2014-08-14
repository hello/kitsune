/*
 * Flc.c - CC31xx/CC32xx Host Driver Implementation, external libs, extlibs/flc - file commit lib
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
#include "extlibs_common.h"
#include "flc_api.h"
#include "bootmgr.h"


void
UARTprintf(const char *pcString, ...);

/* Internal functions */
static _i32 _McuImageGetNewIndex(void);
static _i32 _WriteBootInfo(sBootInfo_t *psBootInfo);
static _i32 _ReadBootInfo(sBootInfo_t *psBootInfo);

/* Save bootinfo on ImageCommit call */
static sBootInfo_t sBootInfo;


_i32 sl_extlib_FlcCommit(_i32 CommitFlag)
{
    _i32 ret_status = 0;

    /* 
     * MCU Commit 
     */
    _ReadBootInfo(&sBootInfo);
        
    /* Check only on status TESTING */
    if( IMG_STATUS_TESTING == sBootInfo.ulImgStatus )
    {
        /* commit TRUE: set status IMG_STATUS_NOTEST and switch to new image */
        if (CommitFlag == FLC_COMMITED)
        {
            UARTprintf("sl_extlib_FlcCommit: Booted in testing mode.\n\r");
            sBootInfo.ulImgStatus = IMG_STATUS_NOTEST;
            sBootInfo.ucActiveImg = (sBootInfo.ucActiveImg == IMG_ACT_USER1)?
                                    IMG_ACT_USER2:
                                    IMG_ACT_USER1;
            /* prepare switch image condition to the MCU boot loader */
            _WriteBootInfo(&sBootInfo);
            
            /* no need to reset the device */
        }
        else
        {
            /* commit FALSE: just reset the MCU to enable the bootloader to rollbacke to the old image */
            ret_status |= FLC_TEST_RESET_MCU;
        }
    }
    
    /* 
     * NWP Commit 
     */
    /* !!! not implemented yet */

    return ret_status;
}

_i32 sl_extlib_FlcTest(_i32 flags)
{
    _i32 ret_status = 0;

    if (flags & FLC_TEST_RESET_MCU)
    {
        /* set status IMG_STATUS_TESTREADY to test the new image */
        UARTprintf("sl_extlib_FlcTest: change image status to IMG_STATUS_TESTREADY\n\r");
        _ReadBootInfo(&sBootInfo);
        sBootInfo.ulImgStatus = IMG_STATUS_TESTREADY;
        _WriteBootInfo(&sBootInfo);
        
        /* reboot, the MCU boot loader should test the new image */
         ret_status |= FLC_TEST_RESET_MCU;
    }
    
    if (flags & FLC_TEST_RESET_NWP)
    {
        /* Image test is not implemented yet, just reset the NWP */
        ret_status |= FLC_TEST_RESET_NWP;
    }
    
    return ret_status;
}

_i32 sl_extlib_FlcIsPendingCommit(void)
{
    _ReadBootInfo(&sBootInfo);
    return (IMG_STATUS_TESTING == sBootInfo.ulImgStatus);
}


_i32 _McuImageGetNewIndex(void)
{
    _i32 newImageIndex;

    /* Assume sBootInfo is alrteady filled in init time (by sl_extlib_FlcCommit) */
    switch(sBootInfo.ucActiveImg)
    {
        case IMG_ACT_USER1:
            newImageIndex = IMG_ACT_USER2;
            break;

        case IMG_ACT_USER2:
        default:
            newImageIndex = IMG_ACT_USER1;
            break;
    }
    UARTprintf("_McuImageGetNewIndex: active image is %d, return new image %d \n\r", sBootInfo.ucActiveImg, newImageIndex);
    
    return newImageIndex;
}

static _i32 _WriteBootInfo(sBootInfo_t *psBootInfo)
{
    _i32 lFileHandle;
    _u32 ulToken;
    _i32 status = -1;

    if( 0 == sl_FsOpen((unsigned char *)IMG_BOOT_INFO, FS_MODE_OPEN_WRITE, &ulToken, &lFileHandle) )
    {
        if( 0 < sl_FsWrite(lFileHandle, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t)) )
        {
            UARTprintf("WriteBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
            status = 0;
        }
        sl_FsClose(lFileHandle, 0, 0, 0);
    }

    return status;
}

static _i32 _ReadBootInfo(sBootInfo_t *psBootInfo)
{
    _i32 lFileHandle;
    _u32 ulToken;
    _i32 status = -1;

    if( 0 == sl_FsOpen((unsigned char *)IMG_BOOT_INFO, FS_MODE_OPEN_READ, &ulToken, &lFileHandle) )
    {
        if( 0 < sl_FsRead(lFileHandle, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t)) )
        {
            status = 0;
            UARTprintf("ReadBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
        }
        sl_FsClose(lFileHandle, 0, 0, 0);
    }

    return status;
}

_i32 sl_extlib_FlcOpenFile(_u8 *file_name, _i32 file_size, _u32 *ulToken, _i32 *lFileHandle, _i32 open_flags)
{
    /* MCU image name should be changed */
    if (strstr((char *)file_name, "/sys/mcuimgA") != NULL)
    {
        file_name[11] = (_u8)_McuImageGetNewIndex() + '1'; /* mcuimg1 is for factory default, mcuimg2,3 are for OTA updates */
        UARTprintf("sl_extlib_FlcOpenFile: MCU image name converted to %s \n", file_name);
    }
    
    if (open_flags == _FS_MODE_OPEN_READ)
        return sl_FsOpen((_u8 *)file_name, _FS_MODE_OPEN_READ, ulToken, lFileHandle);
    else  
        return sl_FsOpen((_u8 *)file_name, FS_MODE_OPEN_CREATE(file_size, open_flags), ulToken, lFileHandle);
}

_i32 sl_extlib_FlcWriteFile(_i32 fileHandle, _i32 offset, char *buf, _i32 len)
{
    return sl_FsWrite(fileHandle, (_u32)(offset), (_u8 *)buf, len);
}

_i32 sl_extlib_FlcReadFile(_i32 fileHandle, _i32 offset, char *buf, _i32 len)
{
    return sl_FsRead(fileHandle, (_u32)(offset), (_u8 *)buf, len);
}

_i32 sl_extlib_FlcCloseFile(_i32 fileHandle, _u8 *pCeritificateFileName,_u8 *pSignature ,_u32 SignatureLen)
{
    if (SignatureLen == 0)
    {
        pSignature = 0;
    }
    return sl_FsClose(fileHandle, pCeritificateFileName, pSignature, SignatureLen);
}

_i32 sl_extlib_FlcAbortFile(_i32 fileHandle)
{
    _u8 abortSig = 'A';
    /* Close the file with signature 'A' which is ABORT */
    return sl_extlib_FlcCloseFile(fileHandle, 0, &abortSig, 1);
}



