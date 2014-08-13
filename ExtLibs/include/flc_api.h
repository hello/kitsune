/*
 * flc_api.h - CC31xx/CC32xx Host Driver Implementation,  external libs, extlibs/flc - file commit lib
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
#ifndef __FLC_H__
#define __FLC_H__

#ifdef    __cplusplus
extern "C" {
#endif

#define FLC_LIB_VERSION    "1.00"

#define FLC_COMMITED      1
#define FLC_NOT_COMMITED  0

#define FLC_TEST_RESET_MCU    0x1
#define FLC_TEST_RESET_NWP    0x2

/*****************************************************************************

    API Prototypes

 *****************************************************************************/

/*!

    \addtogroup Flc
    @{

*/

/*!
    \brief Commit the new updates OTA images (MCU and NWP)

    Check boot info if image status IMG_STATUS_TESTING, change to IMG_STATUS_NOTEST and change also image number (from 1 to 2 or 2 to 1)
    The host should call this function on init time after calling status = slAccess_start_and_connect(AP_SSID).
    If the WLAN_CONNECTED and NET_CONNECTED (IPV4_ACQUIRED) the commit is success

    \param[in] CommitFlag     Commit flag bitmap: \n
                              - FLC_COMMITED (1)
                              - FLC_NOT_COMMITED (0)

    \return                   bitmap of reset recomendation to the host: FLC_TEST_RESET_MCU(1), FLC_TEST_RESET_NWP(2)

    \sa                       
    \note                     
    \warning
    \par                 Example:  
    \code                
    For example: commit after success NWP start  
    
    status = slAccess_start_and_connect(OPEN_LINK_SSID);
    if( 0 > status )
    {
        status = sl_extlib_FlcCommit(FLC_NOT_COMMITED);
        if (status == FLC_TEST_RESET_MCU)
        {
            Report("main: Rebooting...");
            Platform_Reset();
        }
    }

    status = sl_extlib_FlcCommit(IMAGE_COMMITED);
    if (status == FLC_TEST_RESET_MCU)
    {
        Report("main: Rebooting...");
        Platform_Reset();
    }
    
    \endcode
*/
_i32 sl_extlib_FlcCommit(_i32 CommitFlag);

/*!
    \brief Test the new updates OTA images (MCU and NWP)

    There is a new image (MCU/NWP) that need to be tested
    MCU - Set image status to IMG_STATUS_TESTREADY, Setreturn value to FLC_TEST_RESET_MCU
    The host should now reset the MCU so the MCU bootloader will test the new image
    NWP - Test/Commit process is not implemented yet in the NWP, Set return value to FLC_TEST_RESET_NWP
    The host should reset the NWP and test if WLAN_CONNECTED and NET_CONNECTED to commit the image. 
    There is nothing to (no NWP REVERT in R1.0) do in case of no NET_CONNECTED

    \param[in] TestFlag     Commit flag bitmap: \n
                              - FLC_TEST_RESET_MCU (1) - There is a new MCU image that need to be tested before commit
                              - FLC_TEST_RESET_NWP (2)

    \return                   bitmap of reset recomendation to the host: FLC_TEST_RESET_MCU(1), FLC_TEST_RESET_NWP(2)

    \sa                       
    \note                     
    \warning
    \par                 Example:  
    \code                
    For example: test images after OTA download done  
    
    status = sl_extLib_OtaRun(pvOtaApp);
    if ((status & RUN_STAT_DOWNLOAD_DONE))
    {
        if (status & RUN_STAT_DOWNLOAD_DONE_RESET_MCU) ImageTestFlags |= FLC_TEST_RESET_MCU;
        if (status & RUN_STAT_DOWNLOAD_DONE_RESET_MCU) ImageTestFlags |= FLC_TEST_RESET_NWP;
        
        status = sl_extlib_FlcTest(ImageTestFlags);
        if (status & FLC_TEST_RESET_MCU)
        {
            Report("proc_ota_run_step: Rebooting...");
            Platform_Reset();
        }
        if (status & FLC_TEST_RESET_NWP)
        {
            Report("\nproc_ota_run_step: reset the NWP to test the new image ---\n");
            status = slAccess_stop_start(OPEN_LINK_SSID);
        }
    }
    
    \endcode
*/
_i32 sl_extlib_FlcTest(_i32 flags);

/*!
    \brief Check on bootloader flags if the state is IMG_STATUS_TESTING

    \return                   TRUE/FLASE
*/
_i32 sl_extlib_FlcIsPendingCommit(void);

/*!
    \brief Open file for write, rename MCU image name from OTA

    Open file for write, rename MCU image name from OTA
    Only wrapper to sl_FsOpen
    \param[in] Same parameters as sl_FsOpen

    \return                   On success, zero is returned. On error, negative is returned
*/
_i32 sl_extlib_FlcOpenFile(_u8 *file_name, _i32 file_size, _u32 *ulToken, _i32 *lFileHandle, _i32 open_flags);

/*!
    \brief Read file 

    \param[in] Same parameters as sl_FsRead

    \return                   On success, returns the number of read bytes. On error, negative number is returned
*/
_i32 sl_extlib_FlcReadFile(_i32 fileHandle, _i32 offset, char *buf, _i32 len);

/*!
    \brief Write file 

    \param[in] Same parameters as sl_FsWrite

    \return                   On success, returns the number of written bytes. On error, negative number is returned
*/
_i32 sl_extlib_FlcWriteFile(_i32 fileHandle, _i32 offset, char *buf, _i32 len);

/*!
    \brief Close file 

    \param[in] Same parameters as sl_FsClose

    \return                   On success, zero is returned. On error, negative is returned
*/
_i32 sl_extlib_FlcCloseFile(_i32 fileHandle, _u8 *pCeritificateFileName,_u8 *pSignature ,_u32 SignatureLen);

/*!
    \brief Abort file, rollback to the old file (mirror)

    Abort file by closing the file with signature 'A', rollback to the old file (mirror)

    \param[in]      fileHandle   Pointer to the file (assigned from sl_FsOpen) 

    \return                   On success, zero is returned. On error, negative is returned
*/
_i32 sl_extlib_FlcAbortFile(_i32 fileHandle);

#ifdef  __cplusplus
}
#endif /* __cplusplus */

#endif /*  __FLC_H__ */

