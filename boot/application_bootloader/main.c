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
SHA1_CTX sha1ctx;
unsigned char sha[SHA1_SIZE] = {0};

/******************************************************************************
   Image file names
*******************************************************************************/
#define IMG_BOOT_INFO           "/ota/mcubootinfo.bin"
#define IMG_FACTORY_DEFAULT     "/ota/mcuimg1.bin"
#define IMG_USER_1              "/ota/mcuimg2.bin"
#define IMG_USER_2              "/ota/mcuimg3.bin"

/******************************************************************************
   Image status
*******************************************************************************/
#define IMG_STATUS_TESTING      0x12344321
#define IMG_STATUS_TESTREADY    0x56788765
#define IMG_STATUS_NOTEST       0xABCDDCBA

/******************************************************************************
   Active Image
*******************************************************************************/
#define NUM_IMAGES			3

#define IMG_ACT_FACTORY         0
#define IMG_ACT_USER1           1
#define IMG_ACT_USER2           2

//*****************************************************************************
// User image tokens
//*****************************************************************************
#define FACTORY_IMG_TOKEN      0// 0x5555AAAA
#define USER_IMG_1_TOKEN       0// 0xAA5555AA
#define USER_IMG_2_TOKEN       0//0x55AAAA55
#define USER_BOOT_INFO_TOKEN   0// 0xA5A55A5A

#define DEVICE_IS_CC3101RS      0x18
#define DEVICE_IS_CC3101S       0x1B



/******************************************************************************
   Boot Info structure
*******************************************************************************/
typedef struct sBootInfo
{
  _u8  ucActiveImg;
  _u32 ulImgStatus;

  unsigned char sha[NUM_IMAGES][SHA1_SIZE];
}sBootInfo_t;

sBootInfo_t sBootInfo;

//*****************************************************************************
// Local Variables
//*****************************************************************************
static int  iRetVal;
static SlFsFileInfo_t pFsFileInfo;

static unsigned long ulFactoryImgToken;
static unsigned long ulUserImg1Token;
static unsigned long ulUserImg2Token;
static unsigned long ulBootInfoToken;




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
void SimpleLinkHttpServerCallback(SlNetAppHttpServerEvent_t *pHttpEvent,
                                  SlNetAppHttpServerResponse_t *pHttpResponse)
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

void
UARTprintf(const char *pcString, ...){

}
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent){

}
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
}
#include "timer.h"
static unsigned long g_ulTimerA2Base;
void simplelink_timerA2_start()
{
    // Base address for second timer
    //
    g_ulTimerA2Base = TIMERA2_BASE;

    //
    // Configuring the timerA2
    //
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA2,PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_TIMERA2);
    MAP_TimerConfigure(g_ulTimerA2Base,TIMER_CFG_PERIODIC);
    MAP_TimerPrescaleSet(g_ulTimerA2Base,TIMER_A,0);

    /* configure the timer counter load value to max 32-bit value */
    MAP_TimerLoadSet(g_ulTimerA2Base,TIMER_A, 0xFFFFFFFF);
    //
    // Enable the GPT
    //
    MAP_TimerEnable(g_ulTimerA2Base,TIMER_A);


}
void simplelink_timerA2_stop(){
	MAP_TimerDisable(g_ulTimerA2Base,TIMER_A);
}
unsigned long TimerGetCurrentTimestamp()
{
	 return (0xFFFFFFFF - TimerValueGet(g_ulTimerA2Base,TIMER_A));
}
void usertraceMALLOC( void * pvAddress, size_t uiSize ) {

}

void usertraceFREE( void * pvAddress, size_t uiSize ) {

}
void
vAssertCalled( const char * s )
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

extern void (* const g_pfnVectors[])(void);
#pragma DATA_SECTION(ulRAMVectorTable, ".ramvecs")
unsigned long ulRAMVectorTable[256];
static void
BoardInit(void)
{

  //
  // Set vector table base
  //
#ifndef USE_TIRTOS
#if defined(ccs) || defined(gcc)
	memcpy(ulRAMVectorTable,g_pfnVectors,16*4);
#endif

#if defined(ewarm)
	memcpy(ulRAMVectorTable,&__vector_table,16*4);
#endif
	IntVTableBaseSet((unsigned long)&ulRAMVectorTable[0]);
#endif
  //
  // Enable Processor Interrupts
  //
  IntMasterEnable();
  IntEnable(FAULT_SYSTICK);
  simplelink_timerA2_start();
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
#include "flash.h"
#define TRANSFER_BUFFER_SIZE (2048)
static int load_to_flash(long handle, _u32 flash_start, _u32 size){
	int ret = 0;
	_u8 buf[TRANSFER_BUFFER_SIZE];
	_u32 erase_address = flash_start;
	while(ret == 0 && erase_address < (flash_start + size) ){
		FlashErase(erase_address);
		erase_address += 2048;
	}
	if( ret == 0){
		_u32 write_start = flash_start;
		_u32 read_offset = 0;

		while(size){
			_u32 bytes_to_xfer = (size >= TRANSFER_BUFFER_SIZE ) ? TRANSFER_BUFFER_SIZE : size; //allow room to grow to
			ret = sl_FsRead(handle, read_offset, buf, bytes_to_xfer);
			if(ret < 0){
				break;
			}else{
				ret = FlashProgram((unsigned long *)buf, write_start, bytes_to_xfer);
				if(ret != 0){
					break;
				}
				write_start += bytes_to_xfer;
				read_offset += bytes_to_xfer;
				size -= bytes_to_xfer;
			}
		}
	}
	return ret;
}
int Load(unsigned char *ImgName, unsigned long ulToken) {
	//
	// Open the file for reading
	//
	long handle;
	if(0 != sl_FsGetInfo(ImgName, ulToken, &pFsFileInfo)){
		return -1;
	}
	handle = sl_FsOpen(ImgName, SL_FS_READ, &ulToken);
	//
	// Check if successfully opened
	//
	if (handle >= 0) {
		//
		// Get the file size using File Info structure
		//

		file_len = pFsFileInfo.Len;
		//
		// Read the application into SRAM
		//TODO finish this
		/** legacy, for reference
		iRetVal = sl_FsRead(lFileHandle, 0,
				(unsigned char *) APP_IMG_SRAM_OFFSET, pFsFileInfo.Len);
		*/
		iRetVal = load_to_flash(handle, APP_IMG_FLASH_OFFSET, file_len);
		sl_FsClose(handle, 0,0,0);
		return iRetVal;

	}
	return -1;
}
int GetSize(const unsigned char * name){
	SlFsFileInfo_t info = {0};
	if(0 == sl_FsGetInfo(name, 0, &info)){
		return info.Len;
	}else{
		return 0;
	}
}
int Test(unsigned int img, int len) {
	SHA1_Init(&sha1ctx);
	SHA1_Update(&sha1ctx, (unsigned char *) APP_IMG_FLASH_OFFSET, len);
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
    simplelink_timerA2_stop();
    Run(APP_IMG_FLASH_OFFSET);
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
  if((lFileHandle = sl_FsOpen((unsigned char *)IMG_BOOT_INFO, SL_FS_WRITE, &ulToken)) >= 0 )
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
			if (!Test(IMG_ACT_USER2, GetSize((const unsigned char *)IMG_USER_2))) {
				LoadAndExecute((unsigned char *) IMG_USER_1, ulUserImg1Token);
			} else {
				Execute();
			}
			break;

		default:
			Load((unsigned char *) IMG_USER_1, ulUserImg2Token);
			if (!Test(IMG_ACT_USER1, GetSize((const unsigned char *)IMG_USER_1))) {
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
			if (!Test(IMG_ACT_USER1,  GetSize((const unsigned char *)IMG_USER_1))) {
				LoadAndExecute((unsigned char *) IMG_FACTORY_DEFAULT,ulUserImg1Token);
			} else {
				Execute();
			}
			break;

		case IMG_ACT_USER2:
			Load((unsigned char *) IMG_USER_2, ulUserImg2Token);
			if (!Test(IMG_ACT_USER2,  GetSize((const unsigned char *)IMG_USER_2))) {
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

void SimpleLinkSocketTriggerEventHandler(SlSockTriggerEvent_t	*pSlTriggerEvent)
{

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
  // Start slhost to get NVMEM service
  //
  sl_Start(NULL, NULL, NULL);
  sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, SL_WLAN_CONNECTION_POLICY(0, 0, 0, 0), NULL, 0);

  //
  // Open Boot info file for reading
  //
  long handle = sl_FsOpen((unsigned char *)IMG_BOOT_INFO,
		  SL_FS_READ,
		  &ulBootInfoToken);

  //
  // If successful, load the boot info
  // else create a new file with default boot info.
  //
  if( handle >= 0 )
  {
    iRetVal = sl_FsRead(handle,0,
                         (unsigned char *)&sBootInfo,
                         sizeof(sBootInfo_t));

  }
  else
  {

    //
    // Create a new boot info file
    //
	  handle = sl_FsOpen((unsigned char *)IMG_BOOT_INFO,
			  	  SL_FS_CREATE|SL_FS_OVERWRITE| SL_FS_CREATE_MAX_SIZE( 2 * sizeof(sBootInfo_t)),
                 &ulBootInfoToken);

    //
    // Create a default boot info
    //
    iRetVal = CreateDefaultBootInfo(&sBootInfo);

    if(handle < 0)
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
    iRetVal = sl_FsWrite(handle,0,
                         (unsigned char *)&sBootInfo,
                         sizeof(sBootInfo_t));
  }

  //
  // Close boot info function
  //
  sl_FsClose(handle, 0, 0, 0);

  //
  // Load and execute the image base on boot info.
  //
  ImageLoader(&sBootInfo);

}

