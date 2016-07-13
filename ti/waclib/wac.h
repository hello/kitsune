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

#ifndef __WAC_H__
#define __WAC_H__


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
  

#define WAC_VERSION_NUMBER      "1.14" //WAC Implementation version number. Sent over TLV as FW Version.

#define POST_CONFIGURED         "POST /configured"

// HTTP Headers
#define SETUP_RESPONSE           "HTTP/1.1 200 OK\r\n" \
                                 "Content-Type: application/octet-stream\r\n" \
                                 "Content-Length: "
#define CONFIG_RESPONSE          "HTTP/1.1 200 OK\r\n" \
                                 "Content-Type: application/x-tlv8\r\n" \
                                 "Content-Length:"
#define CONFIGURED_RESPONSE      "HTTP/1.1 200 OK\r\n\r\n"
                                   
//HTTP Constants
#define         CONTENT_LENGTH          "Content-Length:"
                                   
// MFi Configuration Features Flags
#define         MFI_CONFIG_APP_ASSOCIATED               1
#define         MFI_CONFIG_TLV_SUPPORTED                4

// MFI Configuration Status Flags
#define         MFI_CONFIG_PROBLEM_DETECTED             1
#define         MFI_CONFIG_DEVICE_NOT_CONFIGURED        2

//WAC mDNS definitions
// _mfi-config._tcp text record strings
#define         DEVICEID                "deviceid="
#define         FEATURES                ";features="
#define         FLAGS                   ";flags="
#define         PROTOVERS               ";protovers="
#define         SEED                    ";seed="
#define         SRCVERS                 ";srcvers="
#define         PROTOCOLVER             "1.0"   //WAC Protocol version number
#define         SOURCEVER               WAC_VERSION_NUMBER   //Our version number

#define         WAC_TTL                 1440
#define         WAC_SERVICE_NAME        "._mfi-config._tcp.local"
#define         WAC_MDNS_SERVICE_TEXT_LEN       100
#define         WAC_PUBLIC_KEY_LEN      32
#define         WAC_PRIVATE_KEY_LEN     32
#define         WAC_SHARED_KEY_LEN      32
#define         WAC_SIGNATURE_LEN       128
#define         WAC_PLAYPASSWD_LEN      33
#define 		MAX_HW_REVISION_LEN     33
                                   
#define         MFI_CERTIFICATE_LEN     1280
#define         MFI_CHALLENGE_DATA_LEN  128
                                   
//TLV definitions
#define         TLV_BUNDLESEEDID        0x01 //Unique 10 character string assigned by Apple to an app via the provisioning portal
#define         TLV_FIRMWAREREVISION    0x02 //Software revision of the accessory
#define         TLV_HARDWAREVERSION     0x03 //Hardware revision of the accessory
#define         TLV_LANGUAGE            0x04 //BCP-47 language to configure the device for
#define         TLV_MANUFACTURER        0x05 //Accessory manufacturer text
#define         TLV_MFIPROTOCOL         0x06 //Revrse-DNS string describing supported MFi accessory protocols (e,g, "com.acme.gadget")
#define         TLV_MODEL               0x07 //Name of the device 
#define         TLV_NAME                0x08 //Name that accessory should use to advertise itself to the user
#define         TLV_PLAYPASSWORD        0x09 //Password used to start an AirPlay stream to the accessory
#define         TLV_SERIALNUMBER        0x0A //Serial number of the accessory
#define         TLV_WIFIPSK             0x0B //WiFi PSK for joining a WPA-protected WiFi network
#define         TLV_WIFISSID            0x0C //Wifi SSID for the accessory to join

// Information Element definitions
#define         WAC_IE_LENGTH           (21 + MAX_DEVICE_URN_LENGTH + (2*MAX_STRING_LENGTH))  //IE comprised of 21 indications of 1 byte, device URN and two user strings (manufacturer, model)
#define         WAC_IE_MODEL_LENGTH     16   //Number of characters

//Flags, higher byte
#define         WAC_IE_FLAG_AIRPLAY_SUPPORT             0x80
#define         WAC_IE_FLAG_DEVICE_UNCONFIGURED         0x40
#define         WAC_IE_FLAG_SUPPORT_MFI_V1              0x20
#define         WAC_IE_FLAG_SUPPORT_WOW                 0x10
#define         WAC_IE_FLAG_INTERFERENCE_ROBUST         0x08
#define         WAC_IE_FLAG_PPPOE_SERVER_DETECTED       0x04
#define         WAC_IE_FLAG_WPS_SUPPORT                 0x02
#define         WAC_IE_FLAG_WPS_ACTIVE                  0x01

//Flags, Middle byte
#define         WAC_IE_FLAG_AIRPRINT_SUPPORT            0x80
#define         WAC_IE_FLAG_2_4G_SUPPORT                0x02
#define         WAC_IE_FLAG_5G_SUPPORT                  0x01

//Flags, Lowest byte
#define         WAC_IE_FLAG_HOMEKIT_SUPPORT             0x40

//TLV constants
#define         TLV_LENGTH              192 //192 bytes total for TLV length. To Do - get buffer from user?
#define         MAX_STRING_LENGTH       64  //Limit user string to 64 bytes
#define         MAX_INFOAPP_LENGTH      10  //Limit BundleSeedID to 10 characters (expected to be exactly 10 characters)
#define         MAX_DEVICE_URN_LENGTH   33  //Currently this is the size limitation in the SimpleLink drvier. IF the limitation changes, update here...
#define         WAC_TIMEOUT             500 //Timeout is set to ~19seconds (37.5mSec * WAC_TIMEOUT)
#define 		WAC_ACK_RECV_TIMEOUT    (5*1000*60)    // (5/10) min. as delay is given 10mSec at every loop
#define         WAC_NO_CONNECT_TIMEOUT  (30*1000*60)  //30 Minutes WAC Idle Timeout  
//#define         WAC_NO_CONNECT_TIMEOUT  (20*60*1000)  //20 Minutes WAC Idle Timeout
#define         WAC_AP_CONNECT_TIMEOUT  10000
#define         WAC_DHCP_TIMEOUT  100000 // 100 sec
                                   
#ifdef SL_LLA_SUPPORT
#define         LLA_ARP_REQ_TIMEOUT             (5*1000) //ARP Request Timeout - 5 Sec
#endif                                   

// Status bits - These are used to set/reset the corresponding bits in 
// given variable
typedef enum{
    WAC_SL_STATUS_BIT_NWP_INIT = 0, // If this bit is set: Network Processor is 
                             // powered up
                             
    WAC_SL_STATUS_BIT_CONNECTION,   // If this bit is set: the device is connected to 
                             // the AP or client is connected to device (AP)
                             
    WAC_SL_STATUS_BIT_IP_LEASED,    // If this bit is set: the device has leased IP to 
                             // any connected client

    WAC_SL_STATUS_BIT_IP_AQUIRED,   // If this bit is set: the device has acquired an IP
    
    WAC_SL_STATUS_BIT_SMARTCONFIG_START, // If this bit is set: the SmartConfiguration 
                                  // process is started from SmartConfig app

    WAC_SL_STATUS_BIT_P2P_DEV_FOUND,    // If this bit is set: the device (P2P mode) 
                                 // found any p2p-device in scan

    WAC_SL_STATUS_BIT_P2P_REQ_RECEIVED, // If this bit is set: the device (P2P mode) 
                                 // found any p2p-negotiation request

    WAC_SL_STATUS_BIT_CONNECTION_FAILED, // If this bit is set: the device(P2P mode)
                                  // connection to client(or reverse way) is failed

    WAC_SL_STATUS_BIT_PING_DONE         // If this bit is set: the device has completed
                                 // the ping operation

}e_WAC_SL_StatusBits;                                   
                                   
#define WAC_GET_NW_STATUS_BIT(status_variable, bit) (0 != (status_variable & (1<<(bit))))


typedef struct 
{
    unsigned char flags;
    unsigned char name[MAX_DEVICE_URN_LENGTH];
    unsigned char macAddress[6];
    unsigned char IE[WAC_IE_LENGTH];
    signed short length;
  
} _sl_ExtLib_WAC_IE_t;

typedef struct 
{
    signed int flags;
    unsigned long wacPortNo;
    unsigned char* wacVendor;
    unsigned char* wacModel;
    unsigned char* wacInfoApp;
    unsigned char* wacMFiProt;
    unsigned char* wacLang;
    unsigned char* wacInfoSerialNumber;
    unsigned char* wacDeviceURN;
    signed short wacVendorSize;
    signed short wacInfoAppSize;
    signed short wacModelSize;
    signed short wacMFiProtSize;
    signed short wacLangSize;
    signed short wacInfoSerialNumberSize;
    signed short wacDeviceURNSize;
    unsigned char wacUserDataSet;
} _sl_ExtLib_WAC_User_Data_t;

typedef struct 
{
    unsigned char firmwareRevision[sizeof(SOURCEVER)];
    //unsigned char hardwareRevision[8];
    unsigned char hardwareRevision[MAX_HW_REVISION_LEN];
    unsigned char name[MAX_DEVICE_URN_LENGTH];
    unsigned char nameLen;
    unsigned char wifiPSK[64];
    unsigned char wifiPSKLen;
    unsigned char wifiSSID[WAC_PLAYPASSWD_LEN];
    unsigned char wifiSSIDLen;
    signed char keyExists;
} sl_ExtLib_WAC_Credentials_t;

typedef struct 
{
    unsigned char deviceConfigured;
    unsigned char WACDone;
    unsigned char  AESKey[16];
    unsigned char  AESIV[16];
    unsigned char TLVData[TLV_LENGTH];
    signed long TLVLength;
    sl_ExtLib_WAC_Credentials_t credentialsStructure;
} _sl_ExtLib_WAC_Data_t;


typedef struct 
{
	signed char	Initialized;
	unsigned char 	playPassword[WAC_PLAYPASSWD_LEN];
    unsigned char playPasswordLen;
} _sl_ExtLib_WAC_GlobalData_t;

enum _sl_ExtLib_WAC_TLV_ID {
    TLV_bundleSeedID = 0x1,
    TLV_firmwareRevision,
    TLV_hardwareRevision, 
    TLV_language, 
    TLV_manufacturer, 
    TLV_mfiProtocol,
    TLV_model, 
    TLV_name, 
    TLV_playPassword, 
    TLV_serialNumber, 
    TLV_wifiPSK, 
    TLV_wifiSSID
} _sl_ExtLib_WAC_TLV_ID;

enum _sl_ExtLib_WAC_IE_ID {
    IE_Flags = 0x0,
    IE_Name = 0x1, 
    IE_Manufacturer = 0x2,
    IE_Model = 0x3,
    IE_OUI = 0x4, 
    IE_MACAddress = 0x7
} _sl_ExtLib_WAC_IE_ID;

/* State machine */
enum _sl_ExtLib_WAC_WACState {
    wsInit = 0,
    wsCredentials,
    wsGetTLV, 
    wsParseTLV,
    wsWaitSocketClose,
    wsApplyCredentials,
    wsConnectStation,
    wsWaitForNewConnection,
    wsFinalizeConfiguration
} SL_WAC_WACState;

typedef struct{
    signed short certificateDataLength; 
    unsigned char  certificateData[MFI_CERTIFICATE_LEN];
    unsigned short challengeDataLength;
    unsigned char  challengeData[MFI_CHALLENGE_DATA_LEN];
} _sl_ExtLib_MFi_Data_t;



//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif
