//*****************************************************************************
// Copyright (C) 2014 Texas Instruments Incorporated
//
// All rights reserved. Property of Texas Instruments Incorporated.
// Restricted rights to use, duplicate or disclose this code are
// granted through contract.
// The program may not be used without the written permission of
// Texas Instruments Incorporated or against the terms and conditions
// stipulated in the agreement under which this program has been supplied,
// and under no circumstances can it be used with non-TI connectivity device.
//
//*****************************************************************************

#ifndef __WACLIB_H__
#define __WACLIB_H__


//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif


//Error definitions
#define		EXTLIB_WAC_SUCCESS      	        0
#define     ERROR_NO_CONTENT_LENGTH 	        -1 //Failed to find content-length string in TLV message
#define     ERROR_NO_TLV_DATA       	        -2 //Failed to find HTTP body in TLV message
#define     ERROR_UNKNOWN_ID_IN_TLV 	        -3 //Unknown (out of spec) data type found in TLV
#define 	ERROR_RESTARTED_IN_AP_MODE	        -4 //After applying credentials, WLAN got restarted in AP mode instead of STA mode
#define 	ERROR_UNKNOWN_STATE     	        -5 //Unknown state in WAC state machine
#define 	ERROR_FAILED_OPENING_SOCKET    	        -6 //Failed to open listening control socket. Shouldn't happen, unless all sockets are used up
#define 	ERROR_FAILED_BINDING_SOCKET    	        -7 //Failed to bind listening control socket
#define 	ERROR_FAILED_LISTENING_SOCKET  	        -8 //Listen command for control socket failed
                                   
#define 	EXTLIB_WAC_ERROR_INVALID_FLAGS          -9 //No valid flags were found
#define 	EXTLIB_WAC_ERROR_INVALID_SIZE           -10 //Invalid size passed to function
#define 	EXTLIB_WAC_ERROR_UNKNOWN_OPERATION      -11 //Unknown operation for set/get function
#define 	EXTLIB_WAC_ERROR_READONLY_OPERATION     -12 //This parameter can only be read, not written
#define 	EXTLIB_WAC_ERROR_REGISTERING_MDNS       -13 //There was an error registering MDNS service
#define 	EXTLIB_WAC_ERROR_TIMEOUT                -14 //Operation has timed out
#define 	EXTLIB_WAC_ERROR_MFI_COMMUNICATION      -15 //Error communicating with MFi chip
#define 	EXTLIB_WAC_ERROR_BAD_PACKET             -16 //Unexpected data recieved from configuring device
#define 	EXTLIB_WAC_ERROR_UNINITIALIZED          -17 //WAC started without proper initialization

#define     EXTLIB_WAC_ERROR_STARTING_MDNS          -18
#define     EXTLIB_WAC_ERROR_IP_ACQ_FAILED          -19
#define     EXTLIB_WAC_ERROR_SEND_DATA_FAILED       -20
#define     EXTLIB_WAC_ERROR_CONTENT_LEN_LIMIT      -21  // if content length is beyond sizeof(int)
#define     ETXLIB_WAC_ERROR_MEM_ALLOC              -22
#define     EXTLIB_WAC_ERROR_BY_NWP                 -23
#define     EXTLIB_WAC_ERROR_NOT_IN_AP_MODE         -24

#define		EXTLIB_WAC_ERROR_INVALID_DEVICE_URN		-25
#define		EXTLIB_WAC_ERROR_INADEQUATE_BUF_LEN		-26
  
#define     EXTLIB_WAC_PORT                49152

//User accesible parameters (get/set functions)
#define         SL_WAC_FLAGS                    1
#define         SL_WAC_VENDOR                   2
#define         SL_WAC_MODEL                    3
#define         SL_WAC_INFO_APP                 4
#define         SL_WAC_INFO_MFIPROT             5
#define         SL_WAC_INFO_LANG                6
#define         SL_WAC_INFO_PLAYPASSWORD        7
#define         SL_WAC_INFO_SERIALNUMBER        8
#define 		SL_WAC_PORT_NO					9
#define 		SL_WAC_DEVICE_URN				10

#define         SL_WAC_AIRPLAY                  0x00000080
#define         SL_WAC_AIRPRINT                 0x00008000
#define         SL_WAC_HOMEKIT                  0x00400000
     

#define SL_WAC_Swap32( X ) \
    ( (unsigned long)( \
        ( ( ( (unsigned long)(X) ) << 24 ) & ( 0xFF000000 ) ) | \
        ( ( ( (unsigned long)(X) ) <<  8 ) & ( 0x00FF0000 ) ) | \
        ( ( ( (unsigned long)(X) ) >>  8 ) & ( 0x0000FF00 ) ) | \
        ( ( ( (unsigned long)(X) ) >> 24 ) & ( 0x000000FF ) ) ) )

  

//*****************************************************************************
//  Externing Global variables in WAC module that can be used by other modules 
//*****************************************************************************
extern volatile int g_currSimplelinkRole; // initial value = -1; will be updated
// appripriately in sl_Start_custom() function
    
//*****************************************************************************
//       Function declarations 
//*****************************************************************************
 

//*****************************************************************************
//
//! \brief Initializes the WAC procedure
//! \par
//! Call this function to prior to starting the WAC procedure. Prior to calling 
//!		the function, you may:
//!        - Set vendor parameters (vendor name, product name, language, etc..)
//! 	   - Set product type (Airplay / Airprint)
//!
//! In case user parameters are not set, the WAC procedure will start as 
//! an Airprint (generic) device, with no extra parameters.  
//!
//!	
//! \param[in]    MFi Address - I2C address of the authentication chip. 
//!                 Can be 0x10 or 0x11.   
//! \param[in]    Device prive key to encrypt/decrypt the data
//!                 if NULL then a key is already hard-coded in WAClib      
//!
//! \return       On success, zero is returned
//!
//! \note         During the WAC procedure, the user may change the device's 
//!               name. In that case, a new URN and AP SSID will be set to 
//!               the device by this function. 					  
//! \warning      
//
//*****************************************************************************
signed long sl_ExtLib_WacInit(unsigned char mfiAddress,
										const unsigned char *devicePrivateKey);


//*****************************************************************************
//
//! \brief      De-Initialize the WAC procedure:
//!             stop mDNS server, reset AP parameters (beacons etc.)
//!
//! \param      Void
//!
//! \return     On success, zero is returned
//
//*****************************************************************************
signed long sl_ExtLib_WacDeInit(void);


//*****************************************************************************
//
//! \brief Starts the WAC procedure
//! \par
//!     Call this function to start the WAC procedure. 
//!
//!     Prior to calling this function, the initialization function must be 
//!     called, and the WLAN has to be started in AP mode. 
//!	
//! \param[in]     Flags                   Reserved for future use. 
//! \param[in]	device private key psecified by user else WAC lib has it's own
//!
//! \return       On success, zero is returned
//!
//! \note         During the WAC procedure, the user may change the device's 
//!                 name. In that case, a new URN and AP SSID will be set to 
//!                 the device by this function. 
//!
//! \warning      Requires a large stack size (~4KB) for data buffer
//
//*****************************************************************************
signed long sl_ExtLib_WacRun(unsigned char ucFlags);



//*****************************************************************************
//
//! \brief Sets WAC user parameters
//! \par
//!       This function sets the user parameters to be used during the WAC 
//!       procedure. The available parameters are:
//!		    - Set user flags (Product type = Airplay / Airprint)
//! 		- Set vendor name string (eg. "Texas Instruments")
//!         - Set model name string (eg. "WiFiSpeaker1,1")
//!         - Set Apple "BundleSeedID" - A unique 10 character string assigned 
//!           by apple to an app via the provisioning protal (e.g. "24D4XFAF43)
//!         - Set MFi Protocol - A reverse-DNS string describing supported 
//!           MFi Accessory protocols (e.g. "com.acme.gadget")
//!         - Set device language - BCP-47 lanaguage code (string)
//!         - Set the device's serial number (string)
//!	
//! \param[in]    type        Parameter type. 
//!               value       Pointer to the beginning of the string or to 
//!                             the variable which contains the flags value. 
//!               size        Integer containing the length of value (for flags,
//!                             length should equal 2 bytes)
//!  
//! \return       On success, zero is returned
//!
//! \note         All parameters set using this function are persistent
//!
//! \warning      
//
//*****************************************************************************
signed long sl_ExtLib_WacSet(unsigned char Option, void* pValue,
									unsigned short Length);

//*****************************************************************************
//! \brief Gets WAC user parameters
//! \par
//!      This function gets the user parameters used during the WAC procedure. 
//!      The available parameters are:
//!         - Get user flags (Product type = Airplay / Airprint)
//!         - Get vendor name string (eg. "Texas Instruments")
//!         - Get model name string (eg. "WiFiSpeaker1,1")
//!         - Get Apple "BundleSeedID" - A unique 10 character string assigned 
//!           by apple to an app via the provisioning protal (e.g. "24D4XFAF43)
//!         - Get MFi Protocol - A reverse-DNS string describing supported MFi 
//!             Accessory protocols (e.g. "com.acme.gadget")
//!         - Get device language - BCP-47 lanaguage code (string)
//!         - Get the device's serial number (string)
//!	
//! \param[in]    type     Parameter type. 
//!               value    Pointer to the beginning of the string or to 
//!                             the variable which contains the flags value. 
//!               size     Pointer to an integer containing the length of value
//!
//! \return       On success, zero is returned
//!
//! \warning      
//*****************************************************************************
signed long sl_ExtLib_WacGet(unsigned char Option, unsigned char* pValue,
									unsigned short* Length);


//*****************************************************************************
// Adding APIs to read and write the device URN name to a global buffer in WAC
// lib. The string in this buffer is used as the device URN name for mDNS WAC
// The strings are retained even after WAC is run, in the buffer accessed
// through these APIs. The same name should be used  for other services,
// after WAC completes. The device name changed during wac is set in the global
// buffer, which can be later on read by the get() API
//*****************************************************************************
int sl_ExtLib_DevURN_WacSet(unsigned char* pValue, unsigned short Length);
int sl_ExtLib_DevURN_WacGet(unsigned char* pValue, unsigned short *Length);

//*****************************************************************************
// this function updates a global variable g_currSimplelinkRole with the role 
// of NWP. the global variable can be used in asynch events etc, where a role
// specific action has be taken.
// This function ensures the global variable updated before being used in asynch
// events. 
// This function is as such blocked till sl_start happens
//*****************************************************************************
void sl_Start_custom(void);


//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif
