/******************************************************************************
*
*   Copyright (C) 2013 Texas Instruments Incorporated
*
*   All rights reserved. Property of Texas Instruments Incorporated.
*   Restricted rights to use, duplicate or disclose this code are
*   granted through contract.
*
*   The program may not be used without the written permission of
*   Texas Instruments Incorporated or against the terms and conditions
*   stipulated in the agreement under which this program has been supplied,
*   and under no circumstances can it be used with non-TI connectivity device.
*
******************************************************************************/
    
#include "simplelink.h"

#ifndef __WLAN_H__
#define	__WLAN_H__

#ifdef	__cplusplus
extern "C" {
#endif

/*!

    \addtogroup wlan
    @{

*/

#define SL_BSSID_LENGTH							(6)
#define MAXIMAL_SSID_LENGTH						(32)

#define NUM_OF_RATE_INDEXES                       20
#define SIZE_OF_RSSI_HISTOGRAM                    6

     
#define SL_SEC_TYPE_OPEN                           0
#define SL_SEC_TYPE_WEP                            1
#define SL_SEC_TYPE_WPA                            2
#define SL_SEC_TYPE_WPS_PBC                        3
#define SL_SEC_TYPE_WPS_PIN                        4
#define SL_SEC_TYPE_WPA_ENT                        5

#define TLS       0x1
#define MSCHAP    0x0
#define PSK       0x2 
#define TTLS      0x10
#define PEAP0     0x20
#define PEAP1     0x40
#define FAST      0x80

#define FAST_AUTH_PROVISIONING      0x02
#define FAST_UNAUTH_PROVISIONING    0x01
#define FAST_NO_PROVISIONING        0x00

#define EAPMETHOD_PHASE2_SHIFT             8
#define EAPMETHOD_PAIRWISE_CIPHER_SHIFT    19
#define EAPMETHOD_GROUP_CIPHER_SHIFT       27

#define WPA_CIPHER_CCMP 0x1 
#define WPA_CIPHER_TKIP 0x2
#define CC31XX_DEFAULT_CIPHER (WPA_CIPHER_CCMP | WPA_CIPHER_TKIP)

#define EAPMETHOD(phase1,phase2,pairwise_cipher,group_cipher) \
((phase1) | \
 ((phase2) << EAPMETHOD_PHASE2_SHIFT ) |\
 ((unsigned long)(pairwise_cipher) << EAPMETHOD_PAIRWISE_CIPHER_SHIFT ) |\
 ((unsigned long)(group_cipher) << EAPMETHOD_GROUP_CIPHER_SHIFT ))

//                                                             phase1  phase2                     pairwise_cipher         group_cipher
#define SL_ENT_EAP_METHOD_TLS                       EAPMETHOD(TLS   , 0                        , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_TTLS_TLS                  EAPMETHOD(TTLS  , TLS                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_TTLS_MSCHAPv2             EAPMETHOD(TTLS  , MSCHAP                   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_TTLS_PSK                  EAPMETHOD(TTLS  , PSK                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP0_TLS                 EAPMETHOD(PEAP0 , TLS                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP0_MSCHAPv2            EAPMETHOD(PEAP0 , MSCHAP                   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER) 
#define SL_ENT_EAP_METHOD_PEAP0_PSK                 EAPMETHOD(PEAP0 , PSK                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP1_TLS                 EAPMETHOD(PEAP1 , TLS                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_PEAP1_MSCHAPv2            EAPMETHOD(PEAP1 , MSCHAP                   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER) 
#define SL_ENT_EAP_METHOD_PEAP1_PSK                 EAPMETHOD(PEAP1 , PSK                      , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_FAST_AUTH_PROVISIONING    EAPMETHOD(FAST  , FAST_AUTH_PROVISIONING   , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_FAST_UNAUTH_PROVISIONING  EAPMETHOD(FAST  , FAST_UNAUTH_PROVISIONING , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
#define SL_ENT_EAP_METHOD_FAST_NO_PROVISIONING      EAPMETHOD(FAST  , FAST_NO_PROVISIONING     , CC31XX_DEFAULT_CIPHER , CC31XX_DEFAULT_CIPHER)
 
typedef enum
{
    CHANNEL_1 = 1,
    CHANNEL_2 = 2,
    CHANNEL_3 = 3,
    CHANNEL_4 = 4,
    CHANNEL_5 = 5,
    CHANNEL_6 = 6,
    CHANNEL_7 = 7,
    CHANNEL_8 = 8,
    CHANNEL_9 = 9,
    CHANNEL_10 = 10,
    CHANNEL_11 = 11,
	CHANNEL_12 = 12,
	CHANNEL_13 = 13,
    MAX_CHANNELS = 0xff
}Channel_e;

#define TI_LONG_PREAMBLE			(0)
#define TI_SHORT_PREAMBLE			(1)

#define SL_RAW_RF_TX_PARAMS_CHANNEL_SHIFT 0
#define SL_RAW_RF_TX_PARAMS_RATE_SHIFT 6
#define SL_RAW_RF_TX_PARAMS_POWER_SHIFT 11
#define SL_RAW_RF_TX_PARAMS_PREAMBLE_SHIFT 15

#define SL_RAW_RF_TX_PARAMS(chan,rate,power,preamble) \
	((chan << SL_RAW_RF_TX_PARAMS_CHANNEL_SHIFT) | \
	(rate << SL_RAW_RF_TX_PARAMS_RATE_SHIFT) | \
	(power << SL_RAW_RF_TX_PARAMS_POWER_SHIFT) | \
	(preamble << SL_RAW_RF_TX_PARAMS_PREAMBLE_SHIFT))

typedef enum
{
    RATE_1M         = 1,
    RATE_2M         = 2,
    RATE_5_5M       = 3,
    RATE_11M        = 4,
    RATE_6M         = 6,
    RATE_9M         = 7,
    RATE_12M        = 8,
    RATE_18M        = 9,
    RATE_24M        = 10,
    RATE_36M        = 11,
    RATE_48M        = 12,
    RATE_54M        = 13,
    RATE_MCS_0      = 14,
    RATE_MCS_1      = 15,
    RATE_MCS_2      = 16,
    RATE_MCS_3      = 17,
    RATE_MCS_4      = 18,
    RATE_MCS_5      = 19,
    RATE_MCS_6      = 20,
    RATE_MCS_7      = 21,
    MAX_NUM_RATES   = 0xFF
}RateIndex_e;

/* wlan config application IDs */
#define SL_WLAN_CFG_AP_ID 0
#define SL_WLAN_CFG_GENERAL_PARAM_ID 1

/* wlan AP Config set/get options */
#define WLAN_AP_OPT_SSID                   0
//#define WLAN_AP_OPT_COUNTRY_CODE           1
#define WLAN_AP_OPT_BEACON_INT             2
#define WLAN_AP_OPT_CHANNEL                3
#define WLAN_AP_OPT_HIDDEN_SSID            4
#define WLAN_AP_OPT_DTIM_PERIOD            5
#define WLAN_AP_OPT_SECURITY_TYPE          6
#define WLAN_AP_OPT_PASSWORD               7
#define WLAN_AP_OPT_WPS_STATE              8
#define WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE  9
#define WLAN_GENERAL_PARAM_OPT_STA_TX_POWER  10
#define WLAN_GENERAL_PARAM_OPT_AP_TX_POWER   11

/* SmartConfig CIPHER options */
#define SMART_CONFIG_CIPHER_SFLASH          0   //password is not delivered by the application. The Simple Manager should 
                                        		//check if the keys are stored in the Flash.
#define SMART_CONFIG_CIPHER_AES             1   //AES (other types are not supported)
#define SMART_CONFIG_CIPHER_NONE         0xFF   //do not check in the flash


#define SL_POLICY_CONNECTION    (0x10)
#define SL_POLICY_SCAN          (0x20)
#define SL_POLICY_PM            (0x30)

#define VAL_2_MASK(position,value)   ((1 & (value))<<(position))
#define MASK_2_VAL(position,mask)    (((1 << position) & (mask)) >> (position))

#define SL_CONNECTION_POLICY(Auto,Fast,Open)                (VAL_2_MASK(0,Auto) | VAL_2_MASK(1,Fast) | VAL_2_MASK(2,Open))
#define SL_SCAN_POLICY_EN(policy)                           (MASK_2_VAL(0,policy))
#define SL_SCAN_POLICY(Enable)                              (VAL_2_MASK(0,Enable))


#define SL_NORMAL_POLICY          (0)
#define SL_LOW_LATENCY_POLICY     (1)
#define SL_LOW_POWER_POLICY       (2)
#define SL_ALWAYS_ON_POLICY       (3)




typedef struct
{
  UINT32    status;
  UINT32    ssid_len;
  UINT8     ssid[32];
  UINT32    private_token_len;
  UINT8     private_token[32];
}slSmartConfigStartAsyncResponse_t;

typedef struct
{
  UINT16    status;
  UINT16    padding;
}slSmartConfigStopAsyncResponse_t;

typedef struct
{
  UINT8     mac[6];
  UINT16    padding;
}slStaConnectedAsyncResponse_t;


typedef union
{
  slSmartConfigStartAsyncResponse_t    smartConfigStartResponse; /*SL_WLAN_SMART_CONFIG_START_EVENT*/
  slSmartConfigStopAsyncResponse_t     smartConfigStopResponse;  /*SL_WLAN_SMART_CONFIG_STOP_EVENT */
  slStaConnectedAsyncResponse_t        staConnected;             /* SL_OPCODE_WLAN_STA_CONNECTED     */
  slStaConnectedAsyncResponse_t        staDisconnected;          /* SL_OPCODE_WLAN_STA_DISCONNECTED  */
} SlWlanEventData_u;

typedef struct
{
   unsigned long     Event;
   SlWlanEventData_u        EventData;
} SlWlanEvent_t;


typedef struct 
{
    UINT32  ReceivedValidPacketsNumber;                     /* sum of the packets that been received OK (include filtered) */
    UINT32  ReceivedFcsErrorPacketsNumber;                  /* sum of the packets that been dropped due to FCS error */ 
    UINT32  ReceivedPlcpErrorPacketsNumber;                 /* sum of the packets that been dropped due to PLCP error */
    INT16   AvarageDataCtrlRssi;                            /* average rssi for all valid data packets received */
    INT16   AvarageMgMntRssi;                               /* average rssi for all valid managment packets received */
    UINT16  RateHistogram[NUM_OF_RATE_INDEXES];             /* rate histogram for all valid packets received */
    UINT16  RssiHistogram[SIZE_OF_RSSI_HISTOGRAM];          /* RSSI histogram from -40 until -87 (all below and above\n rssi will apear in the first and last cells */
    UINT32  StartTimeStamp;                                 /* the time stamp started collecting the statistics in uSec */
    UINT32  GetTimeStamp;                                   /* the time stamp called the get statistics command */
}SlGetRxStatResponse_t;


typedef struct
{
    unsigned char ssid[MAXIMAL_SSID_LENGTH];
    unsigned char ssid_len;
    unsigned char sec_type;
    unsigned char bssid[SL_BSSID_LENGTH];
    char          rssi;
    char          reserved[3];
}Sl_WlanNetworkEntry_t;


 
typedef struct 
{
    unsigned char   Type;
    char*           Key;
    unsigned char   KeyLen;
}SlSecParams_t;
 
typedef struct 
{
    char*           User;
    unsigned char   UserLen;
    char*           AnonUser;
    unsigned char   AnonUserLen;
    unsigned char   CertIndex;  //not supported
    unsigned long   EapMethod;
}SlSecParamsExt_t;
typedef enum
{
    ROLE_STA   =   0,
    ROLE_AP    =   2,
    ROLE_P2P   =   3
}SlWlanMode_t;

#define SL_WLAN_SET_MODE_STA()        sl_WlanSetMode(ROLE_STA)
#define SL_WLAN_SET_MODE_AP()         sl_WlanSetMode(ROLE_AP)
#define SL_WLAN_SET_MODE_P2P()        sl_WlanSetMode(ROLE_P2P)



#define SL_WLAN_AP_SET_SSID(ssid)						{   \
                                                            unsigned char   str[33];        \
                                                            unsigned char   len = strlen(ssid);     \
                                                            memset(str, 0, 33);             \
                                                            memcpy(str, ssid, len);\
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, len, str);  \
                                                        }

#define SL_WLAN_AP_SET_COUNTRY_CODE(country)			{   \
                                                            unsigned char* str = (unsigned char *) country;    \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, 2, str);  \
                                                        }

#define SL_WLAN_AP_SET_BEACON_INTERVAL(interval)        {   \
                                                            unsigned short  val = interval;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_BEACON_INT, 2, (unsigned char *)&val);    \
                                                        }

#define SL_WLAN_AP_SET_CHANNEL(channel)					{   \
                                                            unsigned char  val = channel;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_CHANNEL, 1, (unsigned char *)&val);   \
                                                        };

//0 = disabled
//1 = send empty (length=0) SSID in beacon and ignore probe request for broadcast SSID
//2 = clear SSID (ASCII 0), but keep the original length (this may be required with some 
//    clients that do not support empty SSID) and ignore probe requests for broadcast SSID

#define SL_WLAN_AP_SET_HIDDEN(hidden)					{   \
                                                            unsigned char  val = hidden;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_HIDDEN_SSID, 1, (unsigned char *)&val); \
                                                        }


#define SL_WLAN_AP_SET_DTIM_PERIOD(dtim)				{   \
                                                            unsigned char  val = dtim;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_DTIM_PERIOD, 1, (unsigned char *)&val);    \
                                                        }

#define SL_WLAN_AP_SET_SECURITY_TYPE_OPEN()			    {   \
                                                            unsigned char  val = SL_SEC_TYPE_OPEN;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1, (unsigned char *)&val);  \
                                                        }

#define SL_WLAN_AP_SET_SECURITY_TYPE_WEP()			    {   \
                                                            unsigned char  val = SL_SEC_TYPE_WEP;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1, (unsigned char *)&val);  \
                                                        }

#define SL_WLAN_AP_SET_SECURITY_TYPE_WPA()			    {   \
                                                            unsigned char  val = SL_SEC_TYPE_WPA;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1, (unsigned char *)&val);  \
                                                        }

#define SL_WLAN_AP_SET_PASSWORD(password)				{   \
                                                            unsigned char  str[65];                     \
                                                            unsigned char   len = strlen(password);     \
                                                            memset(str, 0, 65);                         \
                                                            memcpy(str, password, len);                 \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_PASSWORD, len, (unsigned char *)str);  \
                                                        }

//0 = WPS disabled
//1 = WPS enabled, not configured
//2 = WPS enabled, configured
#define SL_WLAN_AP_SET_WPS_STATE(state)					{   \
                                                            unsigned char  val = state;   \
                                                            sl_WlanCfgSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_WPS_STATE, 1, (unsigned char *)&val);  \
                                                        }


// SL_WLAN_AP_SET_DHCP_PARAMS( SL_IPV4_VAL(192,168,1,10), SL_IPV4_VAL(192,168,1,16), 4096)
#define SL_WLAN_AP_SET_DHCP_PARAMS(startIp,endIp,leaseTimeSeconds)  {   \
                                                                        SlNetAppDhcpServerBasicOpt_t dhcpParams;                                   \
                                                                        unsigned char outLen = sizeof(SlNetAppDhcpServerBasicOpt_t);               \
                                                                        dhcpParams.lease_time      = leaseTimeSeconds;                             \
                                                                        dhcpParams.ipv4_addr_start =  startIp;                                     \
                                                                        dhcpParams.ipv4_addr_last  =  endIp;                                       \
                                                                        sl_NetAppStop(SL_NET_APP_DHCP_SERVER_ID);                                  \
                                                                        sl_NetAppSet(SL_NET_APP_DHCP_SERVER_ID, NETAPP_SET_DHCP_SRV_BASIC_OPT, outLen, (unsigned char*)&dhcpParams); \
                                                                        sl_NetAppStart(SL_NET_APP_DHCP_SERVER_ID);                                 \
                                                                    }


//should be 33 bytes long
#define SL_WLAN_AP_GET_SSID(ssid)               { \
	                                                unsigned char len = 33; \
                                                    unsigned char  config_opt = WLAN_AP_OPT_SSID;   \
	                                                sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt , &len, (unsigned char*) ssid); \
	                                            }

#define SL_WLAN_AP_GET_COUNTRY_CODE(country)    { \
	                                                unsigned char len = 2; \
                                                    unsigned char  config_opt = WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE;   \
	                                                sl_WlanCfgGet(SL_WLAN_CFG_GENERAL_PARAM_ID, &config_opt, &len, (unsigned char*) country); \
	                                            }


#define SL_WLAN_AP_GET_BEACON_INTERVAL(interval)	{ \
														unsigned char len = 2; \
                                                        unsigned char  config_opt = WLAN_AP_OPT_BEACON_INT;   \
														sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) interval); \
													}

#define SL_WLAN_AP_GET_CHANNEL(channel)		{ \
													unsigned char len = 1; \
                                                    unsigned char  config_opt = WLAN_AP_OPT_CHANNEL;   \
	                                                sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) channel); \
                                            }

#define SL_WLAN_AP_GET_HIDDEN(hidden)		{ \
													unsigned char len = 1; \
                                                    unsigned char  config_opt = WLAN_AP_OPT_HIDDEN_SSID;   \
	                                                sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) hidden); \
                                            }

#define SL_WLAN_AP_GET_DTIM_PERIOD(dtim)    { \
													unsigned char len = 1; \
                                                    unsigned char  config_opt = WLAN_AP_OPT_DTIM_PERIOD;   \
	                                                sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) dtim); \
											}

#define SL_WLAN_AP_GET_SECURITY_TYPE(sec_type)			{ \
															unsigned char len = 1; \
                                                            unsigned char  config_opt = WLAN_AP_OPT_SECURITY_TYPE;   \
												            sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) sec_type); \
														}

//should be 65 bytes long
#define SL_WLAN_AP_GET_PASSWORD(password)       { \
	                                                unsigned char len = 65; \
                                                    unsigned char  config_opt = WLAN_AP_OPT_PASSWORD;   \
	                                                sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) password); \
	                                            }

#define SL_WLAN_AP_GET_WPS_STATE(state)		{ \
												unsigned char len = 1; \
                                                unsigned char  config_opt = WLAN_AP_OPT_WPS_STATE;   \
	                                            sl_WlanCfgGet(SL_WLAN_CFG_AP_ID, &config_opt, &len, (unsigned char*) state); \
											}
/*
	unsigned long leaseTimeSeconds;                                                                     
	unsigned long startIp;                                                                       
	unsigned long endIp;                                                                                                        
	SL_WLAN_AP_GET_DHCP_PARAMS(&startIp,&endIp,&leaseTimeSeconds); 
	printf("DHCP Start IP %d.%d.%d.%d End IP %d.%d.%d.%d Lease time seconds %d\n",                                                           
			SL_IPV4_BYTE(startIp,3),SL_IPV4_BYTE(startIp,2),SL_IPV4_BYTE(startIp,1),SL_IPV4_BYTE(startIp,0), 
			SL_IPV4_BYTE(endIp,3),SL_IPV4_BYTE(endIp,2),SL_IPV4_BYTE(endIp,1),SL_IPV4_BYTE(endIp,0),         
			leaseTimeSeconds);    
*/
#define SL_WLAN_AP_GET_DHCP_PARAMS(startIp,endIp,leaseTimeSeconds)  {   \
                                                                        SlNetAppDhcpServerBasicOpt_t dhcpParams;                                   \
                                                                        unsigned char outLen = sizeof(SlNetAppDhcpServerBasicOpt_t);               \
                                                                        sl_NetAppGet(SL_NET_APP_DHCP_SERVER_ID, NETAPP_SET_DHCP_SRV_BASIC_OPT, &outLen, (unsigned char*)&dhcpParams); \
                                                                        *leaseTimeSeconds = dhcpParams.lease_time;                                  \
                                                                        *startIp = dhcpParams.ipv4_addr_start ;                                     \
                                                                        *endIp = dhcpParams.ipv4_addr_last;                                         \
                                                                    }



/*!
    \brief Connect to wlan network as a station
    
    \param[in]      sec_type    security types options: \n
                                - SL_SEC_TYPE_OPEN
		                        - SL_SEC_TYPE_WEP
   		                        - SL_SEC_TYPE_WPA
		                        - SL_SEC_TYPE_WPA_ENT
		                        - SL_SEC_TYPE_WPS_PBC
		                        - SL_SEC_TYPE_WPS_PIN
     
    \param[in]      pName       up to 32 bytes in case of STA the name is the SSID of the Access Point
    \param[in]      NameLen     name length
    \param[in]      pMacAddr    6 bytes for MAC address
    \param[in]      pSecParams  Security parameters (use NULL for SL_SEC_TYPE_OPEN)
    \param[in]      pSecExtParams  Enterprise parameters      (use NULL for SL_SEC_TYPE_OPEN)
    
    \return         On success, zero is returned. On error, negative is 
                    returned    
    
    \sa             sl_WlanDisconnect        
    \note           belongs to \ref ext_api       
    \warning        bssid alone is not allowed in this version
                    in this version only single enterprise mode could be used
*/ 
#if _SL_INCLUDE_FUNC(sl_WlanConnect)
int sl_WlanConnect(char* pName, int NameLen, unsigned char *pMacAddr, SlSecParams_t* pSecParams , SlSecParamsExt_t* pSecExtParams);
#endif

/*!
    \brief wlan disconnect
    
    Disconnect connection  
     
    \return         0 disconnected done, other already disconnected
    
    \sa             sl_WlanConnect       
    \note           belongs to \ref ext_api       
    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_WlanDisconnect)
int sl_WlanDisconnect(void);
#endif

/*!
    \brief add profile 
    
    When auto start is enabled, the device connects to a
    station from the profiles table. Up to 7 profiles are
    supported. If several profiles configured the device chose
    the highest priority profile, within each priority group,
    device will chose profile based on security policy, signal
    strength, etc parameters. 


    \param[in]      pName          up to 32 bytes in case of STA the name is the 
	                               SSID of the Access Point
    \param[in]      NameLen     name length
    \param[in]      pMacAddr    6 bytes for MAC address
    \param[in]      pSecParams     security parameters - security type 
	                               (SL_SEC_TYPE_OPEN,SL_SEC_TYPE_WEP,SL_SEC_TYPE_WPA,
					               SL_SEC_TYPE_WPS_PBC,SL_SEC_TYPE_WPS_PIN,
								   SL_SEC_TYPE_WPA_ENT), key, and key length
	\param[in]      pSecExtParams  enterprise parameters - identity, identity length, 
	                               Anonymous, Anonymous length, CertIndex (not supported,
								   certificates need to be placed in a specific file ID),
								EapMethod.

    \param[in]      Priority    profile priority. Lowest priority: 0
    \param[in]      Options     Not supported
     
    \return         On success, profile stored index is returned. On error, -1 is returned 


    \sa             sl_WlanProfileGet , sl_WlanProfileDel       
    \note           belongs to \ref ext_api
    \warning        Only one Enterprise profile is supported.
                    Please Note that in case of adding an existing profile (compared by pName,pMACAddr and security type) 
                    the old profile will be deleted and the same index will be returned.


*/
#if _SL_INCLUDE_FUNC(sl_WlanProfileAdd)
int sl_WlanProfileAdd(char* pName, int NameLen, unsigned char *pMacAddr, SlSecParams_t* pSecParams , SlSecParamsExt_t* pSecExtParams, unsigned long  Priority, unsigned long  Options);
#endif

/*!
    \brief get profile 
    
    read profile from the device     
     
    \param[in]      Index          profile stored index, if index does not exists
								   error is return
    \param[out]     pName          up to 32 bytes, the name of the Access Point
    \param[out]     pNameLen       name length 
    \param[out]     pMacAddr    6 bytes for MAC address
	\param[out]     pSecParams     security parameters - security type 
								   (LAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or
								   WLAN_SEC_WPA2), key and key length are not 
								   return due to security reasons.
	\param[out]     pSecExtParams  enterprise parameters - identity, identity 
								   length, Anonymous, Anonymous length
								CertIndex (not supported), EapMethod.
    \param[out]     Priority       profile priority
								
    
    
     
    \return         On success, 0 is returned. On error, -1 is 
                    returned      
    
    \sa             sl_WlanProfileAdd , sl_WlanProfileDel       
    \note           belongs to \ref ext_api
    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_WlanProfileGet)
int sl_WlanProfileGet(int Index,char* pName, int *pNameLen, unsigned char *pMacAddr, SlSecParams_t* pSecParams, SlSecParamsExt_t* pSecExtParams, unsigned long *pPriority);
#endif

/*!
    \brief Delete WLAN profile
    
    Delete WLAN profile  
     
    \param[in]   index  number of profile to delete   
     
    \return  On success, zero is returned. On error, -1 is 
               returned
    
    \sa   sl_WlanProfileAdd , sl_WlanProfileGet       
    \note           belongs to \ref ext_api
    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_WlanProfileDel)
int sl_WlanProfileDel(int Index);
#endif

/*!
    \brief set policy values
     
    \param[in]      Type      Type of policy to be modified. The Options are:\n 
                              - SL_POLICY_CONNECTION 
                              - SL_POLICY_SCAN 
                              - SL_POLICY_PM
    \param[in]      Policy    The option value which depends on action type
    \param[in]      pVal      An optional value pointer
    \param[in]      ValLen    An optional value length, in bytes
    \return         On success, zero is returned. On error, -1 is 
                    returned   
    \sa             sl_WlanPolicyGet
    \note           belongs to \ref ext_api
    \warning        
    \par            Example: 
    \code
    
    SL_POLICY_CONNECTION type defines three options available to connect the CC31xx device to the AP: \n
    -  If Auto Connect is set, the CC31xx device tries to automatically reconnect to one of its stored profiles, each time the connection fails or the device is rebooted.\n
       To set this option, use sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(1,0,0),NULL,0)\n
    -  If Fast Connect is set, the CC31xx device tries to establish a fast connection to AP. \n
       To set this option, use sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(0,1,0),NULL,0)
    
    SL_POLICY_SCAN defines system scan time interval in case there is no connection. Default interval is 10 minutes. \n
    After settings scan interval, an immediate scan is activated. The next scan will be based on the interval settings. \n
                    -  For example, setting scan interval to 1 minute interval use:
                       unsigned long intervalInSeconds = 60;
		       #define SL_SCAN_ENABLE  1
                       sl_WlanPolicySet(SL_POLICY_SCAN,SL_SCAN_ENABLE, (unsigned char *)&intervalInSeconds,sizeof(intervalInSeconds));

                    -  For example, disable scan:
		       #define SL_SCAN_DISABLE  0
                       sl_WlanPolicySet(SL_POLICY_SCAN,SL_SCAN_DISABLE,0,0);
     
    SL_POLICY_PM defines a power management policy for Station mode only:
                    -  For setting normal power management policy use sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL,0)
                    -  For setting low latency power management policy use sl_WlanPolicySet(SL_POLICY_PM , SL_LOW_LATENCY_POLICY, NULL,0)
                    -  For setting low power management policy use sl_WlanPolicySet(SL_POLICY_PM , SL_LOW_POWER_POLICY, NULL,0)
                    -  For setting always on power management policy use sl_WlanPolicySet(SL_POLICY_PM , SL_ALWAYS_ON_POLICY, NULL,0)
     
    \endcode 
*/

#if _SL_INCLUDE_FUNC(sl_WlanPolicySet)
int sl_WlanPolicySet(unsigned char Type , const unsigned char Policy, unsigned char *pVal,unsigned char ValLen);
#endif
/*!
    \brief get policy values
     
    \param[in]      Type: SL_POLICY_CONNECTION, SL_POLICY_SCAN, SL_POLICY_PM \n
    \param[out]     The returned values, depends on each policy type, will be stored in the allocated buffer pointed by pVal
     
    \return         On success, zero is returned. On error, -1 is 
                    returned   
    \sa             sl_WlanPolicySet
    \note           belongs to \ref ext_api
    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_WlanPolicyGet)
int sl_WlanPolicyGet(unsigned char Type , unsigned char Policy,unsigned char *pVal,unsigned char *pValLen);
#endif
/*!
    \brief Gets the WLAN scan operation results
    
    Gets scan resaults , gets entry from scan result table
     
    \param[in]   Index - Starting index identifier (range 0-19) for getting scan results
    \param[in]   Count - How many entries to fetch. Max is 20-"Index".
    \param[out]  pEntries - pointer to an allocated Sl_WlanNetworkEntry_t. 
                            the number of array items should match "Count" 
     
    \return  Number of valid networks list items
     
    \sa                
    \note       belongs to \ref ext_api
    \warning    This command do not initiate any active scanning action 
    \par        Example:
    \code       An example of fetching max 10 results:
    
                Sl_WlanNetworkEntry_t netEntries[10];
                int resultsCount = sl_WlanGetNetworkList(0,10,&netEntries[0]);
                for(i=0; i< resultsCount; i++)
                {
                      printf("%s\n",netEntries[i].ssid);
                }
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_WlanGetNetworkList)
int sl_WlanGetNetworkList(unsigned char Index, unsigned char Count, Sl_WlanNetworkEntry_t *pEntries);
#endif

/*!
    \brief   Start collecting wlan RX statistics, for unlimited time. 
            
    \return  On success, zero is returned. On error, -1 is returned   
    
    \sa     sl_WlanRxStatStop      sl_WlanRxStatGet
    \note   belongs to \ref ext_api        
    \warning  This API is deprecated and should be removed for next release
    \par        Example:
    \code       Getting wlan RX statistics:             

    void RxStatCollectTwice()
    {
        SlGetRxStatResponse_t rxStat;
        sl_WlanRxStatStart();
        Sleep(1000); // sleep for 1 sec
        sl_WlanRxStatGet(&rxStat,0); // statisticcs has been cleared upon read
        PrintStat(rxStat); //function that prints the statistics (not implemented)
        Sleep(1000); // sleep for 1 sec
        sl_WlanRxStatGet(&rxStat,0); 
        PrintStat(rxStat);
        sl_WlanRxStatStart();               
    }
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_WlanRxStatStart)
int sl_WlanRxStatStart(void);
#endif


/*!
    \brief    Stop collecting wlan RX statistic, (if previous called sl_WlanRxStatStart)
            
    \return   On success, zero is returned. On error, -1 is returned   
    
    \sa     sl_WlanRxStatStart      sl_WlanRxStatGet
    \note           belongs to \ref ext_api        
    \warning  This API is deprecated and should be removed for next release   
    \par        Example:
    \code       Getting wlan RX statistics:             
 
    void RxStatCollectTwice()
    {
        SlGetRxStatResponse_t rxStat;
        sl_WlanRxStatStart();
        Sleep(1000); // sleep for 1 sec
        sl_WlanRxStatGet(&rxStat,0); // statisticcs has been cleared upon read
        PrintStat(rxStat); //function that prints the statistics (not implemented)
        Sleep(1000); // sleep for 1 sec
        sl_WlanRxStatGet(&rxStat,0); 
        PrintStat(rxStat);
        sl_WlanRxStatStart();               
    }
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_WlanRxStatStop)
int sl_WlanRxStatStop(void);
#endif


/*!
    \brief Get wlan RX statistics. upon calling this command, the statistics counters will be cleared.
            
    \param[in]  Flags should be 0  ( not applicable right now, will be added the future )
    \param[in]  pRxStat a pointer to SlGetRxStatResponse_t filled with Rx statistics results
    \return     On success, zero is returned. On error, -1 is returned   
    
    \sa   sl_WlanRxStatStart  sl_WlanRxStatStop  
    \note           belongs to \ref ext_api        
    \warning     
    \par        Example:
    \code       Getting wlan RX statistics:             
  
 
    void RxStatCollectTwice()
    {
        SlGetRxStatResponse_t rxStat;
        sl_WlanRxStatStart();
        Sleep(1000); // sleep for 1 sec
        sl_WlanRxStatGet(&rxStat,0); // statisticcs has been cleared upon read
        PrintStat(rxStat); //function that prints the statistics (not implemented)
        Sleep(1000); // sleep for 1 sec
        sl_WlanRxStatGet(&rxStat,0); 
        PrintStat(rxStat);
        sl_WlanRxStatStart();               
    }
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_WlanRxStatGet)
int sl_WlanRxStatGet(SlGetRxStatResponse_t *pRxStat,unsigned long Flags);
#endif


/*!
    \brief Stop Smart Config procedure. Once Smart Config will be stopped,
           Asynchronous event will be received - SL_OPCODE_WLAN_SMART_CONFIG_STOP_ASYNC_RESPONSE.
            
    \param[in]  none
    \param[out] none
    
    \return     0 - if Stop Smart Config is about to be executed without errors.
    
    \sa         sl_WlanSmartConfigStart         
    \note           belongs to \ref ext_api        
    \warning      This API is deprecated and should be removed for next release   
    
*/
#if _SL_INCLUDE_FUNC(sl_WlanSmartConfigStop)
int sl_WlanSmartConfigStop(void);
#endif



/*!
    \brief  Start Smart Config procedure. The target of the procedure is to let the                      \n
            device to gain the network parameters: SSID and Password (if network is secured)             \n
            and to connect to it once located in the network range.                                      \n
            An external application should be used on a device connected to any mobile network.          \n
            The external application will transmit over the air the network parameters in secured manner.\n
            The Password may be decrypted using a Key.                                                   \n
            The decryption method may be decided in the command or embedded in the Flash.                \n
            The procedure can be activated for 1-3 group ID in the range of BIT_0 - BIT_15 where the default group ID id 0 (BIT_0) \n
            Once Smart Config has ended successfully, Asynchronous event will be received -              \n
            SL_OPCODE_WLAN_SMART_CONFIG_START_ASYNC_RESPONSE.                                            \n
            The Event will hold the SSID and an extra field that might have been delivered as well (i.e. - device name)

    \param[in]  groupIdBitmask - each bit represent a group ID that should be searched.
                                 The Default group ID id BIT_0. 2 more group can be searched 
                                 in addition. The range is BIT_0 - BIT_15.
    \param[in]   chiper - 0: check in flash, 1 - AES, 0xFF - do not check in flash
    \param[in]  publicKeyLen - public key len (used for the default group ID - BIT_0)
    \param[in]  group1KeyLen - group ID1 length
    \param[in]  group2KeyLen - group ID2 length
    \param[in]  publicKey    - public key (used for the default group ID - BIT_0)
    \param[in]  group1Key    - group ID1 key
    \param[in]  group2Key    - group ID2 key

    \param[out] none
    
    \return     0 - if Smart Config started successfully.
    
    \sa         sl_WlanSmartConfigStop 
    \note           belongs to \ref ext_api        
    \warning     
    \par     
    \code       An example of starting smart Config on group ID's 0 + 1 + 2
    
                sl_WlanSmartConfigStart(7,      //group ID's (BIT_0 | BIT_1 | BIT_2)
                                        1,      //decrypt key by AES method
                                        16,     //decryption key length for group ID0
                                        16,     //decryption key length for group ID1
                                        16,     //decryption key length for group ID2
                                        "Key0Key0Key0Key0", //decryption key for group ID0
                                        "Key1Key1Key1Key1", //decryption key for group ID1
                                        "Key2Key2Key2Key2"  //decryption key for group ID2
                                        );
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_WlanSmartConfigStart)
int sl_WlanSmartConfigStart(const unsigned long    groupIdBitmask,
                             const unsigned char    cipher,
                             const unsigned char    publicKeyLen,
                             const unsigned char    group1KeyLen,
                             const unsigned char    group2KeyLen,
                             const unsigned char*   publicKey,
                             const unsigned char*   group1Key,
                             const unsigned char*   group2Key);
#endif


/*!
    \brief Wlan set mode

    Setting WLAN mode

    \param[in] mode - WLAN mode to start the CC31xx device. Possible options are:
                    - ROLE_STA - for WLAN station mode
                    - ROLE_AP  - for WLAN AP mode
    
    \return   0 - if mode was set correctly   
    \sa        sl_Start sl_Stop
    \note           belongs to \ref ext_api        
    \warning   After setting the mode the system must be restarted for activating the new mode  
    \par       Example:
    \code                
    
    For example: Setting WLAN AP mode:
                 sl_WlanSetMode(ROLE_AP);                
                 sl_Stop(1);                                                                     
                 sl_Start(NULL,NULL,NULL);
                 
    \endcode
*/
#if _SL_INCLUDE_FUNC(sl_WlanSetMode)
int sl_WlanSetMode(const unsigned char    mode);
#endif


/*!
    \brief     Internal function for setting WLAN configurations

    \return    On success, zero is returned. On error, -1 is 
               returned
   
    \param[in] ConfigId -  configuration id

    \param[in] ConfigOpt - configurations option

    \param[in] ConfigLen - configurations len

    \param[in] pValues -   configurations values

    \sa         

    \note 

    \warning     
*/
#if _SL_INCLUDE_FUNC(sl_WlanCfgSet)
int sl_WlanCfgSet(unsigned char ConfigId ,unsigned char ConfigOpt,unsigned char ConfigLen, unsigned char *pValues);
#endif

/*!
    \brief     Internal function for getting WLAN configurations

    \return    On success, zero is returned. On error, -1 is 
               returned
   
    \param[in] ConfigId -  configuration id

    \param[out] pConfigOpt - get configurations option 

    \param[out] pConfigLen - The length of the allocated memory as input, when the
                                        function complete, the value of this parameter would be
                                        the len that actually read from the device. 
                                        If the device return length that is longer from the input 
                                        value, the function will cut the end of the returned structure
                                        and will return 1


    \param[out] pValues - get configurations values

    \sa         

    \note 

    \warning     
*/

#if _SL_INCLUDE_FUNC(sl_WlanCfgGet)
int sl_WlanCfgGet(unsigned char ConfigId, unsigned char *pConfigOpt,unsigned char *pConfigLen, unsigned char *pValues);
#endif
/*

 Close the Doxygen group.
 @}

 */


#ifdef  __cplusplus
}
#endif // __cplusplus

#endif	// __WLAN_H__
