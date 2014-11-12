//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_gprcm.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "simplelink.h"
#include "interrupt.h"
#include "gpio.h"
#include "udma_if.h"
#include "bootmgr.h"

#include "crypto.h"
#define SHA1_SIZE 32
SHA1_CTX sha1ctx;
unsigned char sha[SHA1_SIZE] = {0};

/******************************************************************************
   Image file names
*******************************************************************************/
#define IMG_BOOT_INFO           "/sys/mcubootinfo.bin"
#define IMG_FACTORY_DEFAULT     "/sys/mcuimg1.bin"
#define IMG_USER_1              "/sys/mcuimg2.bin"
#define IMG_USER_2              "/sys/mcuimg3.bin"

/******************************************************************************
   Image status
*******************************************************************************/
#define IMG_STATUS_TESTING      0x12344321
#define IMG_STATUS_TESTREADY    0x56788765
#define IMG_STATUS_NOTEST       0xABCDDCBA

/******************************************************************************
   Active Image
*******************************************************************************/
#define NUM_OTA_IMAGES			2

#define IMG_ACT_FACTORY         0
#define IMG_ACT_USER1           1
#define IMG_ACT_USER2           2

//*****************************************************************************
// User image tokens
//*****************************************************************************
#define FACTORY_IMG_TOKEN       0x5555AAAA
#define USER_IMG_1_TOKEN        0xAA5555AA
#define USER_IMG_2_TOKEN        0x55AAAA55
#define USER_BOOT_INFO_TOKEN    0xA5A55A5A

#define DEVICE_IS_CC3101RS      0x18
#define DEVICE_IS_CC3101S       0x1B



/******************************************************************************
   Boot Info structure
*******************************************************************************/
typedef struct sBootInfo
{
  _u8  ucActiveImg;
  _u32 ulImgStatus;

  unsigned char sha[NUM_OTA_IMAGES][SHA1_SIZE];
}sBootInfo_t;

sBootInfo_t sBootInfo;

//*****************************************************************************
// Local Variables
//*****************************************************************************
static long lFileHandle;
static int  iRetVal;
static SlFsFileInfo_t pFsFileInfo;

static unsigned long ulFactoryImgToken;
static unsigned long ulUserImg1Token;
static unsigned long ulUserImg2Token;
static unsigned long ulBootInfoToken;
static unsigned long ulBootInfoCreateFlag;




//*****************************************************************************
// Vector Table
#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


//*****************************************************************************
// WLAN Event handler callback hookup function
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{


}

//*****************************************************************************
// HTTP Server callback hookup function
//*****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{

}

//*****************************************************************************
// Net APP Event callback hookup function
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{

}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{

}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{

  //
  // Set vector table base
  //
#if defined(ccs) || defined(gcc)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif

#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif

  //
  // Enable Processor Interrupts
  //
  IntMasterEnable();
  IntEnable(FAULT_SYSTICK);

  //
  // Mandatory MCU Initialization
  //
  PRCMCC3200MCUInit();
}

#include "rom_map.h"
#include "wdt.h"
#include "wdt_if.h"
void WatchdogIntHandler(void)
{
	//
	// watchdog interrupt - if it fires when the interrupt has not been cleared then the device will reset...
	//
}
void start_wdt() {
#define WD_PERIOD_MS 				20000
#define MAP_SysCtlClockGet 			80000000
#define LED_GPIO             		MCU_RED_LED_GPIO	/* RED LED */
#define MILLISECONDS_TO_TICKS(ms) 	((MAP_SysCtlClockGet / 1000) * (ms))
    //
    // Enable the peripherals used by this example.
    //
    MAP_PRCMPeripheralClkEnable(PRCM_WDT, PRCM_RUN_MODE_CLK);

    //
    // Set up the watchdog interrupt handler.
    //
    WDT_IF_Init(WatchdogIntHandler, MILLISECONDS_TO_TICKS(WD_PERIOD_MS));

    //
    // Start the timer. Once the timer is started, it cannot be disable.
    //
    MAP_WatchdogEnable(WDT_BASE);
    if(!MAP_WatchdogRunning(WDT_BASE))
    {
       WDT_IF_DeInit();
    }
}
//*****************************************************************************
//
//! Executed the application from given location
//!
//! \param ulBaseLoc is the base address of the application
//!
//! This function execution the application loaded at \e ulBaseLoc. It assumes
//! the vector table is placed at the base address thus sets the new Stack
//! pointer from the first word.
//!
//! \return None.
//
//*****************************************************************************
#ifndef ccs
void Run(unsigned long ulBaseLoc)
{

  //
  // Set the SP
  //
  __asm("	ldr    sp,[r0]\n"
	"	add    r0,r0,#4");

  //
  // Jump to entry code
  //
  __asm("	ldr    r1,[r0]\n"
        "	bx     r1");
}
#else
__asm("    .sect \".text:Run\"\n"
      "    .clink\n"
      "    .thumbfunc Run\n"
      "    .thumb\n"
      "Run:\n"
      "    ldr    sp,[r0]\n"
      "    add    r0,r0,#4\n"
      "    ldr    r1,[r0]\n"
      "    bx     r1");
#endif


//*****************************************************************************
//
//! Load the application from sFlash and execute
//!
//! \param ImgName is the name of the application image on sFlash
//! \param ulToken is the token for reading file (relevant on secure devices only)
//!
//! This function loads the specified application from sFlash and executes it.
//!
//! \return None.
//
//*****************************************************************************
int file_len = 0;
int Load(unsigned char *ImgName, unsigned long ulToken) {
	//
	// Open the file for reading
	//
	iRetVal = sl_FsOpen(ImgName, FS_MODE_OPEN_READ, &ulToken, &lFileHandle);
	//
	// Check if successfully opened
	//
	if (0 == iRetVal) {
		//
		// Get the file size using File Info structure
		//
		iRetVal = sl_FsGetInfo(ImgName, ulToken, &pFsFileInfo);
		file_len = pFsFileInfo.FileLen;
		//
		// Check for failure
		//
		if (0 == iRetVal) {

			//
			// Read the application into SRAM
			//
			iRetVal = sl_FsRead(lFileHandle, 0,
					(unsigned char *) APP_IMG_SRAM_OFFSET, pFsFileInfo.FileLen);
		}
	}
	return iRetVal != pFsFileInfo.FileLen;
}
int Test(unsigned int img) {
	SHA1_Init(&sha1ctx);
	SHA1_Update(&sha1ctx, (unsigned char *) APP_IMG_SRAM_OFFSET, file_len);
	SHA1_Final(sha, &sha1ctx);

	return memcmp(sha, sBootInfo.sha[img], SHA1_SIZE) == 0;
}
void Execute() {
    //
    // Stop the network services
    //
    sl_Stop(30);

    //
    // Execute the application.
    //
    start_wdt(); //if we do load something bad, the wdt will get us back and we can load the other image...
    Run(APP_IMG_SRAM_OFFSET);
}
void LoadAndExecute(unsigned char *ImgName, unsigned long ulToken)
{
	if (Load(ImgName, ulToken) == 0) {
		Execute();
	}
}


//*****************************************************************************
//
//! Writes into the boot info file.
//!
//! \param psBootInfo is pointer to the boot info structure.
//!
//! This function writes the boot info into the boot info file in the sFlash
//!
//! \return Return 0 on success, -1 otherwise.
//
//*****************************************************************************
static long BootInfoWrite(sBootInfo_t *psBootInfo)
{
  long lFileHandle;
  unsigned long ulToken;

  //
  // Open the boot info file for write
  //
  if( 0 == sl_FsOpen((unsigned char *)IMG_BOOT_INFO, FS_MODE_OPEN_WRITE,
                      &ulToken, &lFileHandle) )
  {
    //
    // Write the boot info
    //
    if( 0 < sl_FsWrite(lFileHandle,0, (unsigned char *)psBootInfo,
                         sizeof(sBootInfo_t)) )
    {

    //
    // Close the file
    //
    sl_FsClose(lFileHandle, 0, 0, 0);

    //
    // Return success
    //
    return 0;
   }

  }

  //
  // Return failure
  //
  return -1;
}

//*****************************************************************************
//
//! Load the proper image based on information from boot info and executes it.
//!
//! \param psBootInfo is pointer to the boot info structure.
//!
//! This function loads the proper image based on information from boot info
//! and executes it. \e psBootInfo should be properly initialized.
//!
//! \return None.
//
//*****************************************************************************
static void ImageLoader(sBootInfo_t *psBootInfo)
 {
	unsigned char ucActiveImg;
	unsigned long ulImgStatus;

	//
	// Get the active image and image status
	//
	ucActiveImg = psBootInfo->ucActiveImg;
	ulImgStatus = psBootInfo->ulImgStatus;

	//
	// Boot image based on image status and active image configuration
	//
	if ( IMG_STATUS_TESTREADY == ulImgStatus) {
		//
		// Some image waiting to be tested; Change the status to testing
		// in boot info file
		//
		psBootInfo->ulImgStatus = IMG_STATUS_TESTING;
		BootInfoWrite(psBootInfo);

		//
		// Boot the test image ( the non-active image )
		//
		switch (ucActiveImg) {

		case IMG_ACT_USER1:
			Load((unsigned char *) IMG_USER_2, ulUserImg2Token);
			if (!Test(IMG_ACT_USER2)) {
				LoadAndExecute((unsigned char *) IMG_USER_1, ulUserImg1Token);
			} else {
				Execute();
			}
			break;

		default:
			Load((unsigned char *) IMG_USER_1, ulUserImg2Token);
			if (!Test(IMG_ACT_USER1)) {
				LoadAndExecute((unsigned char *) IMG_USER_2, ulUserImg1Token);
			} else {
				Execute();
			}
		}
	} else {
		if ( IMG_STATUS_TESTING == ulImgStatus) {
			//
			// Something went wrong while in testing.
			// Change the status to no test
			//
			psBootInfo->ulImgStatus = IMG_STATUS_NOTEST;
			BootInfoWrite(psBootInfo);
		}
		//
		// Since boot the acive image.
		//
		switch (ucActiveImg) {

		case IMG_ACT_USER1:
			Load((unsigned char *) IMG_USER_1, ulUserImg2Token);
			if (!Test(IMG_ACT_USER1)) {
				LoadAndExecute((unsigned char *) IMG_FACTORY_DEFAULT,ulUserImg1Token);
			} else {
				Execute();
			}
			break;

		case IMG_ACT_USER2:
			Load((unsigned char *) IMG_USER_2, ulUserImg2Token);
			if (!Test(IMG_ACT_USER2)) {
				LoadAndExecute((unsigned char *) IMG_FACTORY_DEFAULT,ulUserImg1Token);
			} else {
				Execute();
			}
			break;

		default:
			LoadAndExecute((unsigned char *) IMG_FACTORY_DEFAULT,ulFactoryImgToken);
			break;
		}
	}

	//
	// Boot info might be corrupted just try for the factory one
	//
	LoadAndExecute((unsigned char *) IMG_FACTORY_DEFAULT, ulFactoryImgToken);

}

//*****************************************************************************
//
//! Checks if the device is secure
//!
//! This function checks if the device is a secure device or not.
//!
//! \return Returns \b true if device is secure, \b false otherwise
//
//*****************************************************************************
static inline tBoolean IsSecureMCU()
{
  unsigned long ulChipId;

  ulChipId =(HWREG(GPRCM_BASE + GPRCM_O_GPRCM_EFUSE_READ_REG2) >> 16) & 0x1F;

  if((ulChipId != DEVICE_IS_CC3101RS) &&(ulChipId != DEVICE_IS_CC3101S))
  {
    //
    // Return non-Secure
    //
    return false;
  }

  //
  // Return secure
  //
  return true;
}

//*****************************************************************************
//
//!\internal
//!
//! Creates default boot info structure
//!
//! \param psBootInfo is pointer to boot info structure to be initialized
//!
//! This function initializes the boot info structure \e psBootInfo based on
//! application image(s) found on sFlash storage. The default boot image is set
//! to one of Factory, User1 or User2 image in the same priority order.
//!
//! \retunr Returns 0 on success, -1 otherwise.
//
//*****************************************************************************
static int CreateDefaultBootInfo(sBootInfo_t *psBootInfo)
{

    //
    // Set the status to no test
    //
    psBootInfo->ulImgStatus = IMG_STATUS_NOTEST;

    //
    // Check if factor default image exists
    //
    iRetVal = sl_FsGetInfo((unsigned char *)IMG_FACTORY_DEFAULT, 0,&pFsFileInfo);
    if(iRetVal == 0)
    {
      psBootInfo->ucActiveImg = IMG_ACT_FACTORY;
      return 0;
    }

    iRetVal = sl_FsGetInfo((unsigned char *)IMG_USER_1, 0,&pFsFileInfo);
    if(iRetVal == 0)
    {
      psBootInfo->ucActiveImg = IMG_ACT_USER1;
      return 0;
    }

    iRetVal = sl_FsGetInfo((unsigned char *)IMG_USER_2, 0,&pFsFileInfo);
    if(iRetVal == 0)
    {
      psBootInfo->ucActiveImg = IMG_ACT_USER2;
      return 0;
    }

    return -1;
}


//*****************************************************************************
//
//! Main function
//!
//! This is the main function for this application bootloader
//!
//! \return None
//
//*****************************************************************************
int main()
{
  //
  // Board Initialization
  //
  BoardInit();

  //
  // Initialize the DMA
  //
  UDMAInit();

  //
  // Default configuration
  //
  sBootInfo.ucActiveImg = IMG_ACT_FACTORY;
  sBootInfo.ulImgStatus = IMG_STATUS_NOTEST;

  //
  // Initialize boot info file create flag
  //
  ulBootInfoCreateFlag  = _FS_FILE_OPEN_FLAG_COMMIT|_FS_FILE_PUBLIC_WRITE;

  //
  // Check if its a secure MCU
  //
  if ( IsSecureMCU() )
  {
    ulFactoryImgToken     = FACTORY_IMG_TOKEN;
    ulUserImg1Token       = USER_IMG_1_TOKEN;
    ulUserImg2Token       = USER_IMG_2_TOKEN;
    ulBootInfoToken       = USER_BOOT_INFO_TOKEN;
    ulBootInfoCreateFlag  = _FS_FILE_OPEN_FLAG_COMMIT|_FS_FILE_OPEN_FLAG_SECURE|
                            _FS_FILE_OPEN_FLAG_NO_SIGNATURE_TEST|
                            _FS_FILE_PUBLIC_WRITE|_FS_FILE_OPEN_FLAG_VENDOR;
  }


  //
  // Start slhost to get NVMEM service
  //
  sl_Start(NULL, NULL, NULL);

  //
  // Open Boot info file for reading
  //
  iRetVal = sl_FsOpen((unsigned char *)IMG_BOOT_INFO,
                        FS_MODE_OPEN_READ,
                        &ulBootInfoToken,
                        &lFileHandle);

  //
  // If successful, load the boot info
  // else create a new file with default boot info.
  //
  if( 0 == iRetVal )
  {
    iRetVal = sl_FsRead(lFileHandle,0,
                         (unsigned char *)&sBootInfo,
                         sizeof(sBootInfo_t));

  }
  else
  {

    //
    // Create a new boot info file
    //
    iRetVal = sl_FsOpen((unsigned char *)IMG_BOOT_INFO,
                        FS_MODE_OPEN_CREATE(2*sizeof(sBootInfo_t),
                                            ulBootInfoCreateFlag),
                                            &ulBootInfoToken,
                                            &lFileHandle);

    //
    // Create a default boot info
    //
    iRetVal = CreateDefaultBootInfo(&sBootInfo);

    if(iRetVal != 0)
    {
      //
      // Can't boot no bootable image found
      //
      while(1)
      {

      }
    }

    //
    // Write the default boot info.
    //
    iRetVal = sl_FsWrite(lFileHandle,0,
                         (unsigned char *)&sBootInfo,
                         sizeof(sBootInfo_t));
  }

  //
  // Close boot info function
  //
  sl_FsClose(lFileHandle, 0, 0, 0);

  //
  // Load and execute the image base on boot info.
  //
  ImageLoader(&sBootInfo);

}

