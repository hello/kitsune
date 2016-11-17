#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kit_assert.h"
#include <stdint.h>

#include "uartstdio.h"

#include "wlan.h"
#include "socket.h"
#include "simplelink.h"
#include "protocol.h"
#include "sl_sync_include_after_simplelink_header.h"

#include "ble_proto.h"
#include "wifi_cmd.h"
#include "networktask.h"


#define ROLE_INVALID (-5)

int sl_mode = ROLE_INVALID;


#include "rom_map.h"
#include "prcm.h"
#include "utils.h"

#include "hw_memmap.h"
#include "rom_map.h"
#include "gpio.h"
#include "led_cmd.h"

#include "hw_wdt.h"
#include "wdt.h"
#include "wdt_if.h"

#include "FreeRTOS.h"
#include "task.h"

#include "sys_time.h"
#include "kitsune_version.h"

#define FAKE_MAC 0

#include "rom_map.h"
#include "rom.h"
#include "interrupt.h"
#include "uart_logger.h"

#include "proto_utils.h"
#include "ustdlib.h"

#include "pill_settings.h"

#include "fs_utils.h"

#include "limits.h"
#include "hw_ver.h"
#include "audio_features_upload_task.h"

int32_t set_volume(int v, unsigned int dly);
int send_top(char *, int);
void mcu_reset()
{
	set_volume(0, 1000);
	codec_mute_spkr();
	uart_logger_flush_err_shutdown();
	send_top("bounce", strlen("bounce"));
	vTaskDelay(1000);
	LOGI("did not get power reset\r\n");
	//TODO make flush work on reset...
	//MAP_IntMasterDisable();
	//uart_logger_flush();
#define SLOW_CLK_FREQ           (32*1024)
    //
    // Configure the HIB module RTC wake time
    //
    MAP_PRCMHibernateIntervalSet(5 * SLOW_CLK_FREQ);
    //
    // Enable the HIB RTC
    //
    MAP_PRCMHibernateWakeupSourceEnable(PRCM_HIB_SLOW_CLK_CTR);

    //DBG_PRINT("HIB: Entering HIBernate...\n\r");
    UtilsDelay(80000);
    //
    // Enter HIBernate mode
    //
    MAP_PRCMHibernateEnter();
}

#define SL_STOP_TIMEOUT                 (30)
long nwp_reset() {
	long r;
	sl_enter_critical_region();
    sl_WlanSetMode(ROLE_STA);
	r = sl_Stop(0xFF);
    wifi_status_set(0xFFFFFFFF, true);
    r = sl_Start(NULL, NULL, NULL);
	sl_exit_critical_region();
	return r;
}



//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlNetAppHttpServerEvent_t *pHttpEvent,
                                  SlNetAppHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
}

static uint8_t _connected_ssid[MAX_SSID_LEN];


// TODO static uint8_t _connected_bssid[BSSID_LEN];
void wifi_get_connected_ssid(uint8_t* ssid_buffer, size_t len)
{
    size_t copy_len = MAX_SSID_LEN > len ? len : MAX_SSID_LEN;
    memcpy(ssid_buffer, _connected_ssid, copy_len - 1);
}
//****************************************************************************
//
//!    \brief This function handles WLAN events
//!
//! \param  evnt is the event passed to the handler
//!
//! \return None
//
//****************************************************************************
static void wifi_update_task( void * params ) {
	ble_reply_wifi_status((wifi_connection_state)params);
	vTaskDelete(NULL);
}


void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
	switch (slFatalErrorEvent->Id)
	{
		case SL_DEVICE_EVENT_FATAL_DEVICE_ABORT:
		{
			LOGE("[ERROR] - FATAL ERROR: Abort NWP event detected: AbortType=%d, AbortData=0x%x\n\r",slFatalErrorEvent->Data.DeviceAssert.Code,slFatalErrorEvent->Data.DeviceAssert.Value);
		}
		break;

		case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
		{
			LOGE("[ERROR] - FATAL ERROR: Driver Abort detected. \n\r");
		}
		break;

		case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
		{
			LOGE("[ERROR] - FATAL ERROR: No Cmd Ack detected [cmd opcode = 0x%x] \n\r", slFatalErrorEvent->Data.NoCmdAck.Code);
		}
		break;

		case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
		{
			LOGE("[ERROR] - FATAL ERROR: Sync loss detected n\r");
		}
		break;

		case SL_DEVICE_EVENT_FATAL_CMD_TIMEOUT:
		{
			LOGE("[ERROR] - FATAL ERROR: Async event timeout detected [event opcode =0x%x]  \n\r", slFatalErrorEvent->Data.CmdTimeout.Code);
		}
		break;

	default:
		LOGE("[ERROR] - FATAL ERROR: Unspecified error detected \n\r");
			break;
	}
	nwp_reset();
}


void SimpleLinkWlanEventHandler(SlWlanEvent_t *evnt) {
    switch (evnt->Id) {
    case SL_WLAN_EVENT_CONNECT:
    {
        wifi_status_set(CONNECT, false);
        wifi_status_set(CONNECTING, true);
        char* pSSID = (char*)evnt->Data.Connect.SsidName;
        uint8_t ssidLength = evnt->Data.Connect.SsidLen;
        if (ssidLength > MAX_SSID_LEN) {
        	LOGE("ssid tooo long\n");
		}else{
			memset(_connected_ssid, 0, MAX_SSID_LEN);
			memcpy(_connected_ssid, pSSID, ssidLength);
		}
        LOGI("SL_WLAN_CONNECT_EVENT\n");
		xTaskCreate(wifi_update_task, "wifi_update_task", 1024 / 4, (void*)wifi_connection_state_WLAN_CONNECTED, 1, NULL);
    }
    break;
    case SL_WLAN_EVENT_DISCONNECT:
        wifi_status_set(0xFFFFFFFF, true);
        memset(_connected_ssid, 0, MAX_SSID_LEN);
        LOGI("SL_WLAN_DISCONNECT_EVENT\n");

#if 0
        { //recommended ti debug block
		int i;

		LOGI("AP: \"%s\" Code=%d BSSID:",_connected_ssid, evnt->Data.STAandP2PModeDisconnected.reason_code);
    	LOGI( "%x", _connected_bssid[0]);
    	for( i=1;i< BSSID_LEN;++i) {
        	LOGI( ":%x", _connected_bssid[i] );
        } LOGI("\n");
        }
#endif
		xTaskCreate(wifi_update_task, "wifi_update_task", 1024 / 4, (void*)wifi_connection_state_NO_WLAN_CONNECTED, 1, NULL);

	break;
	case SL_WLAN_EVENT_STA_ADDED:
	break;
	case SL_WLAN_EVENT_STA_REMOVED:
	break;
	case SL_WLAN_EVENT_PROVISIONING_STATUS:
	break;
	case SL_WLAN_EVENT_PROVISIONING_PROFILE_ADDED:
	break;
    default:
        wifi_status_set(CONNECTING, true);
        LOGI("UNEXPECTED WLAN EVENT %d\n", evnt->Id);
        nwp_reset();
    }
}
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
// TODO LOGI("GENEVT ID=%d Sender=%d\n",pDevEvent->Data.deviceEvent.status, pDevEvent->Data.deviceEvent.sender);
}
//****************************************************************************
//
//!    \brief This function handles events for IP address acquisition via DHCP
//!           indication
//!
//! \param  pNetAppEvent is the event passed to the Handler
//!
//! \return None
//
//****************************************************************************


void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {

    switch (pNetAppEvent->Id) {
	case SL_NETAPP_EVENT_IPV4_ACQUIRED:
	case SL_NETAPP_EVENT_IPV6_ACQUIRED:
		LOGI("SL_NETAPP_IPV4_ACQUIRED\n\r");
		{
			int seed = (unsigned) PRCMSlowClkCtrGet();
			LOGI("seeding %d\r\n", seed);
			srand(seed); //seed with low bits of lf clock when connecting(not sure when it happens, gives some more entropy).
		}

		wifi_status_set(HAS_IP, false);

		xTaskCreate(wifi_update_task, "wifi_update_task", 1024 / 4, (void*)wifi_connection_state_IP_RETRIEVED, 1, NULL);
		break;

	case SL_NETAPP_EVENT_DHCPV4_LEASED:
		wifi_status_set(IP_LEASED, false);
        break;
    default:
        break;
    }
}

void reset_default_antenna()
{
	int err = sl_FsDel((unsigned char*)ANTENNA_FILE, 0);
	if (err) {
		LOGI("226error %d\n", err);
	}
}

static char account_id[40] = "nolinkedaccount";

void save_account_id( char * acct ) {
	memcpy(account_id, acct, strlen(acct)+1);
	fs_save( ACCOUNT_ID_FILE, acct, strlen(acct)+1 );
}

void save_default_antenna( unsigned char a ) {
	fs_save(ANTENNA_FILE, &a, 1);
}

int load_account_id( ) {
	return fs_get( ACCOUNT_ID_FILE, account_id, sizeof(account_id), NULL );
}
extern volatile bool use_dev_server;
void load_data_server(){
	char dummy[2] = {0};
	fs_get(SERVER_SELECTION_FILE, dummy, sizeof(dummy), NULL);
	if(dummy[0] == '1'){
		UARTprintf("\r\n===============\r\n");
		UARTprintf("Using Dev Endpoint");
		UARTprintf("\r\n===============\r\n");
		use_dev_server = true;
	}
}

int Cmd_setDev(int argc, char *argv[]) {
	if(argc > 1){
		if(argv[1][0] == '1'){
			DISP("Setting Dev\r\n");
			fs_save(SERVER_SELECTION_FILE, "1", 2);
		}else if(argv[1][0] == '0'){
			DISP("Removing Dev\r\n");
			fs_save(SERVER_SELECTION_FILE, "0", 2);
		}
		DISP("Restart to apply changes\r\n");
	}
	return 0;
}

char * get_account_id() {
	return account_id;
}

unsigned char get_default_antenna() {
	unsigned char a = PCB_ANT;

	if( 0 > fs_get( ANTENNA_FILE, &a, 1, NULL ) ) {
		return PCB_ANT;
	}
	return a;
}

void antsel(unsigned char a)
{
	if (get_hw_ver() < EVT1_1p5) {
		if (a == 1) {
			MAP_GPIOPinWrite(GPIOA3_BASE, 0xC, 0x8);
		} else if (a == 2) {
			MAP_GPIOPinWrite(GPIOA3_BASE, 0xC, 0x4);
		}
	}
	return;
}
int Cmd_antsel(int argc, char *argv[]) {
    if (argc != 2) {
    	LOGF( "usage: antsel <1=IFA or 2=chip>\n\r");
        return -1;
    }
    antsel( *argv[1] ==  '1' ? 1 : 2 );

    return 0;
}

int Cmd_country(int argc, char *argv[]) {
	LOGF("country <code, either US, JP, or EU>\n");
	if (argc != 2) {
	    _u16 len = 4;
	    _u16  config_opt = SL_WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE;
	    _u8 cc[4];
	    sl_WlanGet(SL_WLAN_CFG_GENERAL_PARAM_ID, &config_opt, &len, (_u8* )cc);
		LOGF("Country code %s\n",cc);
		return 0;
	}

	sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
			SL_WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, 2, (uint8_t*)argv[1]);

	nwp_reset();
	return 0;
}

#ifdef BUILD_IPERF
int Cmd_iperf_server(int argc, char *argv[]) {
    if (argc != 3) {
        LOGI( "usage: iperfsvr <port> <num packets>\n\r");
        return -1;
    }
    SlSockAddrIn_t  sAddr;
    SlSockAddrIn_t  sLocalAddr;
    int             iCounter;
    int             iAddrSize;
    int             iSockID;
    int             iStatus;
    int             iNewSockID;
    long            lLoopCount = 0;
    long            lNonBlocking = 1;
    int             iTestBufLen;

#define BUF_SIZE            1400

    uint8_t buf[BUF_SIZE];
    // filling the buffer
    for (iCounter=0 ; iCounter<BUF_SIZE ; iCounter++)
    {
        buf[iCounter] = (char)(iCounter % 10);
    }

    unsigned short port = atoi(argv[1]);
    unsigned int packets = atoi(argv[2]);
    iTestBufLen  = BUF_SIZE;

    //filling the TCP server socket address
    sLocalAddr.sin_family = SL_AF_INET;
    sLocalAddr.sin_port = sl_Htons((unsigned short)port);
    sLocalAddr.sin_addr.s_addr = 0;

    // creating a TCP socket
    iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( iSockID < 0 )
    {
        // error
    	return -1;
    }

    iAddrSize = sizeof(SlSockAddrIn_t);

    // binding the TCP socket to the TCP server address
    iStatus = sl_Bind(iSockID, (SlSockAddr_t *)&sLocalAddr, iAddrSize);
    if( iStatus < 0 )
    {
        // error
        (sl_Close(iSockID));
        LOGE("failed to receive\n");
        return -1;
    }

    // putting the socket for listening to the incoming TCP connection
    iStatus = sl_Listen(iSockID, 0);
    if( iStatus < 0 )
    {
        (sl_Close(iSockID));
        LOGE("failed to receive\n");
        return -1;
    }

    // setting socket option to make the socket as non blocking
    iStatus = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_NONBLOCKING,
                            &lNonBlocking, sizeof(lNonBlocking));
    if( iStatus < 0 )
    {
        (sl_Close(iSockID));
        LOGE("failed to receive\n");
        return -1;
    }
    iNewSockID = SL_EAGAIN;

    unsigned char ucDHCP = 0;
    unsigned char len = sizeof(SlNetCfgIpV4Args_t);
    SlNetCfgIpV4Args_t ipv4 = { 0 };
    sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO, &ucDHCP, &len,
            (unsigned char *) &ipv4);
    LOGI("run iperf command \"iperf.exe -c "
                            "%d.%d.%d.%d -p %d -i 1 -t 100000\" \n\r",
                            SL_IPV4_BYTE(ipv4.ipV4,3),
                            SL_IPV4_BYTE(ipv4.ipV4,2),
                            SL_IPV4_BYTE(ipv4.ipV4,1),
                            SL_IPV4_BYTE(ipv4.ipV4,0), port);

    // waiting for an incoming TCP connection
    while( iNewSockID < 0 )
    {
        // accepts a connection form a TCP client, if there is any
        // otherwise returns SL_EAGAIN
        iNewSockID = sl_AcceptNoneThreadSafe(iSockID, ( struct SlSockAddr_t *)&sAddr, (SlSocklen_t*)&iAddrSize);
        if( iNewSockID == SL_EAGAIN )
        {
           vTaskDelay(10);
        }
        else if( iNewSockID < 0 )
        {
            // error
            ( sl_Close(iNewSockID));
            (sl_Close(iSockID));
            LOGE("failed to receive\n");
            return -1;
        }
    }

    // waits for 1000 packets from the connected TCP client
    while (lLoopCount < packets)
    {
        iStatus = sl_Recv(iNewSockID, buf, iTestBufLen, 0);
        if( iStatus <= 0 )
        {
          // error
          ( sl_Close(iNewSockID));
          (sl_Close(iSockID));
          LOGE("failed to receive\n");
          break;
        }

        lLoopCount++;
    }


    // close the connected socket after receiving from connected TCP client
    (sl_Close(iNewSockID));

    // close the listening socket
    (sl_Close(iSockID));

    LOGI("Recieved %u packets successfully\n\r",lLoopCount);


    return 0;
}
int Cmd_iperf_client(int argc, char *argv[]) {
    if (argc != 4) {
    	LOGI("Run iperf command on target server \"iperf.exe -s -i 1\"\n");
        LOGI( "usage: iperfcli <ip (hex)> <port> <packets>\n\r");
        return -1;
    }

    int             iCounter;
    short           sTestBufLen;
    SlSockAddrIn_t  sAddr;
    int             iAddrSize;
    int             iSockID;
    int             iStatus;
    long            lLoopCount = 0;
    uint8_t buf[BUF_SIZE];

    // filling the buffer
    for (iCounter=0 ; iCounter<BUF_SIZE ; iCounter++)
    {
        buf[iCounter] = (char)(iCounter % 10);
    }

    sTestBufLen  = BUF_SIZE;

    unsigned int packets = atoi(argv[3]);
    unsigned short port = atoi(argv[2]);
    char * pend;
    unsigned int ip = strtol(argv[1], &pend, 16);

    //filling the TCP server socket address
    sAddr.sin_family = SL_AF_INET;
    sAddr.sin_port = sl_Htons((unsigned short)port);
    sAddr.sin_addr.s_addr = ((unsigned int)ip);

    iAddrSize = sizeof(SlSockAddrIn_t);

    // creating a TCP socket
    iSockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( iSockID < 0 )
    {
    	LOGE("failed to create socket");
    	return -1;
    }

    // connecting to TCP server
    iStatus = sl_Connect(iSockID, ( SlSockAddr_t *)&sAddr, iAddrSize);
    if( iStatus < 0 )
    {
        // error
        sl_Close(iSockID);
    	LOGE("failed to connect socket");
    	return -1;
    }

    // sending multiple packets to the TCP server
    while (lLoopCount < packets)
    {
        // sending packet
        iStatus = sl_Send(iSockID, buf, sTestBufLen, 0 );
        if( iStatus <= 0 )
        {
            // error
            LOGE("failed to send to socket");
        	break;
        }
        lLoopCount++;
    }

    LOGI("Sent %u packets successfully\n\r",lLoopCount);

    //closing the socket after sending 1000 packets
    sl_Close(iSockID);

    return 0;
}
#endif

int get_wifi_scan_result(SlWlanNetworkEntry_t* entries, uint16_t entry_len, uint32_t scan_duration_ms, int antenna)
{
    if(scan_duration_ms < 1000)
    {
        return 0;
    }

    unsigned long IntervalVal = 20;

    unsigned char policyOpt = SL_WLAN_CONNECTION_POLICY(0,0,0,0);
    int r;

    if( antenna ) {
    	antsel(antenna);
    }

    r = sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION , policyOpt, NULL, 0);

    // Make sure scan is enabled
    policyOpt = SL_WLAN_SCAN_POLICY(1,1);

    // set scan policy - this starts the scan
    r = sl_WlanPolicySet(SL_WLAN_POLICY_SCAN , policyOpt, (unsigned char *)(IntervalVal), sizeof(IntervalVal));


    // delay specific milli seconds to verify scan is started
    vTaskDelay(scan_duration_ms);

    // r indicates the valid number of entries
    // The scan results are occupied in netEntries[]
    r = sl_WlanGetNetworkList(0, entry_len, entries);

    // Restore connection policy to Auto
    sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION, SL_WLAN_CONNECTION_POLICY(1, 1, 0, 0), NULL, 0);

    return r;

}

void wifi_reset()
{
    int16_t ret = sl_WlanProfileDel(0xFF);
    if(ret)
    {
        LOGI("Delete all stored endpoint failed, error %d.\n", ret);
    }else{
        LOGI("All stored WIFI EP removed.\n");
    }

    ret = sl_WlanDisconnect();
    if(ret == 0){
        LOGI("WIFI disconnected");
    }else{
        LOGI("Disconnect WIFI failed, error %d.\n", ret);
    }

    while(wifi_status_get(CONNECT))
    {
    	LOGI("Waiting disconnect...\n");
    	vTaskDelay(1000);
    }

    reset_default_antenna();
}

int Cmd_disconnect(int argc, char *argv[]) {

    wifi_reset();
    return (0);
}
int Cmd_connect(int argc, char *argv[]) {
	int idx;

    if (argc != 4) {
    	LOGF(
                "usage: connect <ssid> <key> <security: 0=open, 1=wep, 2=wpa>\n\r");
    }
    connect_wifi( argv[1], argv[2], atoi(argv[3]), 1,  &idx, true );
    return (0);
}
void set_backup_dns() {
    SlNetCfgIpV4DnsClientArgs_t DnsOpt;

   DnsOpt.DnsSecondServerAddr  =  SL_IPV4_VAL(8,8,4,4);
   sl_NetCfgSet(SL_NETCFG_IPV4_DNS_CLIENT,0,sizeof(SlNetCfgIpV4DnsClientArgs_t),(unsigned char *)&DnsOpt);
}
int Cmd_status(int argc, char *argv[]) {
    _u16 len = sizeof(SlNetCfgIpV4Args_t);
    _u16 ConfigOpt = 0;   //return value could be one of the following: SL_NETCFG_ADDR_DHCP / SL_NETCFG_ADDR_DHCP_LLA / SL_NETCFG_ADDR_STATIC
    SlNetCfgIpV4Args_t ipV4 = {0};
    sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE,&ConfigOpt,&len,(_u8 *)&ipV4);

	LOGF("DHCP is %s IP %d.%d.%d.%d MASK %d.%d.%d.%d GW %d.%d.%d.%d DNS %d.%d.%d.%d\n",
        (ConfigOpt == SL_NETCFG_ADDR_DHCP) ? "ON" : "OFF",
        SL_IPV4_BYTE(ipV4.Ip,3),SL_IPV4_BYTE(ipV4.Ip,2),SL_IPV4_BYTE(ipV4.Ip,1),SL_IPV4_BYTE(ipV4.Ip,0),
        SL_IPV4_BYTE(ipV4.IpMask,3),SL_IPV4_BYTE(ipV4.IpMask,2),SL_IPV4_BYTE(ipV4.IpMask,1),SL_IPV4_BYTE(ipV4.IpMask,0),
        SL_IPV4_BYTE(ipV4.IpGateway,3),SL_IPV4_BYTE(ipV4.IpGateway,2),SL_IPV4_BYTE(ipV4.IpGateway,1),SL_IPV4_BYTE(ipV4.IpGateway,0),
        SL_IPV4_BYTE(ipV4.IpDnsServer,3),SL_IPV4_BYTE(ipV4.IpDnsServer,2),SL_IPV4_BYTE(ipV4.IpDnsServer,1),SL_IPV4_BYTE(ipV4.IpDnsServer,0));


    return 0;
}

// callback routine
void pingRes(SlNetAppPingReport_t* pLOGI) {
    // handle ping results
    LOGF(
            "Ping tx:%d rx:%d min time:%d max time:%d avg time:%d test time:%d\n",
            pLOGI->PacketsSent, pLOGI->PacketsReceived,
            pLOGI->MinRoundTime, pLOGI->MaxRoundTime, pLOGI->AvgRoundTime,
            pLOGI->TestTime);
}

int Cmd_ping(int argc, char *argv[]) {
    static SlNetAppPingReport_t report;
    SlNetAppPingCommand_t pingCommand;

    pingCommand.Ip = SL_IPV4_VAL(192, 168, 1, 1); // destination IP address is my router's IP
    pingCommand.PingSize = 32;                     // size of ping, in bytes
    pingCommand.PingIntervalTime = 100;   // delay between pings, in miliseconds
    pingCommand.PingRequestTimeout = 1000; // timeout for every ping in miliseconds
    pingCommand.TotalNumberOfAttempts = 3; // max number of ping requests. 0 - forever
    pingCommand.Flags = 1;                        // LOGI after each ping

    sl_NetAppPing(&pingCommand, SL_AF_INET, &report, pingRes);
    return (0);
}

int Cmd_time(int argc, char*argv[]) {
	uint32_t unix = fetch_ntp_time_from_ntp()-2208988800UL;
	uint32_t t = get_time();

    LOGF("time is %u and the ntp is %u and the diff is %d, good time? %d\n", t, unix, (int)(t-unix), has_good_time());

    return 0;
}
int Cmd_mode(int argc, char*argv[]) {
    int ap = 0;
    if (argc != 2) {
        LOGF("mode <1=ap 0=station>\n");
    }
    ap = atoi(argv[1]);
    if (ap && sl_mode != ROLE_AP) {
        //Switch to AP Mode
        sl_WlanSetMode(ROLE_AP);
        nwp_reset();
    }
    if (!ap && sl_mode != ROLE_STA) {
        //Switch to STA Mode
        nwp_reset();
    }

    return 0;
}
#include "crypto.h"
uint8_t aes_key[AES_BLOCKSIZE + 1] = DEFAULT_KEY;
static uint8_t device_id[DEVICE_ID_SZ + 1];

int save_aes( uint8_t * key ) {
	save_aes_in_memory(key);
	return fs_save( AES_KEY_LOC, key, AES_BLOCKSIZE);
}
int save_aes_in_memory(const uint8_t * key ) {
	memcpy( aes_key, key, AES_BLOCKSIZE);
	return 0;
}
bool has_default_key() {
	return 0 == memcmp(aes_key, DEFAULT_KEY, AES_BLOCKSIZE);
}
static volatile bool burn_top_key = false;
bool should_burn_top_key() {
	return burn_top_key;
}
extern volatile bool top_got_device_id;
int Cmd_burn_top(int argc, char *argv[]) {
	burn_top_key = true;
	if (top_got_device_id) {
		send_top("rst", strlen("rst"));
	}
	return 0;
}


int get_aes(uint8_t * dst){
	memcpy(dst, aes_key, AES_BLOCKSIZE);
	return 0;
}
int save_device_id( uint8_t * new_device_id ) {
	return fs_save( DEVICE_ID_LOC, new_device_id, DEVICE_ID_SZ);
}

#if 1
int Cmd_set_aes(int argc, char *argv[]) {
    static uint8_t key[AES_BLOCKSIZE + 1] = "1234567891234567";

    save_aes( key );
	// Return success.
	return (0);
}
#endif


int Cmd_set_mac(int argc, char*argv[]) {
    uint8_t MAC_Address[6];
    int i;
    char* pend;
    char* next = &argv[1][0];

    for( i=0; i<6;++i) {
        MAC_Address[i] = strtol(next, &pend, 16);
        next = pend+1;
    }

    sl_NetCfgSet(SL_NETCFG_MAC_ADDRESS_SET,1,SL_MAC_ADDR_LEN,(_u8 *)MAC_Address);
    nwp_reset();

    return 0;
}

void load_aes() {
	int r;

	fs_get( AES_KEY_LOC, aes_key, AES_BLOCKSIZE, &r );
	aes_key[AES_BLOCKSIZE] = 0;

	if (r != AES_BLOCKSIZE) {
		LOGE("failed to read aes key file\n");
		return;
	}

	/*
	int i;
	UARTprintf("AES block loaded from file: ");
	for(i = 0; i < AES_BLOCKSIZE; i++){
		UARTprintf("%02X", aes_key[i]);
	}
	UARTprintf("\n");
	*/
}
#include "netcfg.h"
bool load_device_id() {
	static bool loaded = false;
	if( !loaded ) {
		int r;

		int ret = fs_get( DEVICE_ID_LOC, device_id, DEVICE_ID_SZ, &r );
		device_id[DEVICE_ID_SZ] = 0;

		if (r != DEVICE_ID_SZ || ret < 0) {
			LOGE("failed to read device id file\n");
			return false;
		} else {
			loaded = true;
		}
	}
	/*
	UARTprintf("device id loaded from file: ");
	int i;
	for(i = 0; i < DEVICE_ID_SZ; i++)
	{
		UARTprintf("%02X", device_id[i]);
	}

	UARTprintf("\n");
	*/
	return true;
}
void set_mac_to_device_id() {
	if( load_device_id() ) {
		unsigned char mac[6] = {0x5c,0x6b,0x4f,0,0,0};
		mac[3] = device_id[DEVICE_ID_SZ-3];
		mac[4] = device_id[DEVICE_ID_SZ-2];
		mac[5] = device_id[DEVICE_ID_SZ-1];
		sl_NetCfgSet(SL_NETCFG_MAC_ADDRESS_SET, 1, SL_MAC_ADDR_LEN, mac);
		nwp_reset();
	}
}

static void _on_morpheus_command_success(void * structdata){
	LOGF("signature validated\r\n");
}
static void _on_morpheus_command_failure(){
    LOGF("signature validation fail\r\n");
}

int Cmd_test_key(int argc, char*argv[]) {
    load_aes();
    load_device_id();
    //LOGI("Last two digit: %02X:%02X\n", aes_key[AES_BLOCKSIZE - 2], aes_key[AES_BLOCKSIZE - 1]);
    int ret;
    protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = ble_proto_get_morpheus_command;
    pb_cb.free_reply_pb = ble_proto_free_morpheus_command;
    pb_cb.on_pb_failure = _on_morpheus_command_failure;
    pb_cb.on_pb_success = _on_morpheus_command_success;

	MorpheusCommand * test_command = (MorpheusCommand*)pvPortMalloc(sizeof(MorpheusCommand));
	memset(test_command, 0, sizeof(MorpheusCommand));
	test_command->type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE;
	test_command->version = PROTOBUF_VERSION;

	ret = NetworkTask_SendProtobuf(false, DATA_SERVER, CHECK_KEY_ENDPOINT,
			MorpheusCommand_fields, test_command, INT_MAX, NULL,
			test_command, &pb_cb, true );
    if(ret != 0)
    {
        // network error
    	wifi_status_set(UPLOADING, true);
        LOGI("Send data failed, network error %d\n", ret);
    }
    return ret;
}

/* protobuf includes */
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "periodic.pb.h"
#include "audio_data.pb.h"

typedef struct {
	intptr_t fd;
	uint32_t inbuf_offset;
	uint8_t * buf;
	int32_t buf_pos;
	int32_t buf_size;
	SHA1_CTX * ctx;
	uint32_t bytes_written;
	uint32_t bytes_that_should_have_been_written;
} ostream_buffered_desc_t;

#if 0
static bool write_callback_sha(pb_ostream_t *stream, const uint8_t *buf,
        size_t count) {

	ostream_buffered_desc_t * desc = (ostream_buffered_desc_t *) stream->state;


    SHA1_Update(desc->ctx, buf, count);

    return send(desc->fd, buf, count, 0) == count;
}
#endif

int send_chunk_len( int obj_sz, int sock ) {
	#define CL_BUF_SZ 12
	int rv;
	char recv_buf[CL_BUF_SZ] = {0};
	if( obj_sz == 0 ) {
		usnprintf(recv_buf, CL_BUF_SZ, "\r\n%x\r\n\r\n", obj_sz);
	} else {
		usnprintf(recv_buf, CL_BUF_SZ, "\r\n%x\r\n", obj_sz);
	}
	LOGD("CL:%s", recv_buf);
	rv = send(sock, recv_buf, strlen(recv_buf), 0);
	if (rv != strlen(recv_buf)) {
		LOGI("Sending CE failed %d %d\n", rv, strlen(recv_buf));
		return -1;
	}

 //   LOGI("HTTP chunk sent %s", recv_buf);
	return 0;
}
static void log_debug_bytes( char * tag, uint8_t * bytes, int len ) {
	int i = 0;
	LOGD(tag);
	LOGD("%02x",bytes[i++] );
	for(;i<len;++i) {
		LOGD(":%02x",bytes[i] );
	}
	LOGD("\n");
}

static bool flush_out_buffer(ostream_buffered_desc_t * desc ) {
	uint32_t buf_size = desc->buf_pos;
	bool ret = true;

	if (buf_size > 0) {
		desc->bytes_written += buf_size;

		//encrypt
		SHA1_Update(desc->ctx, desc->buf, buf_size);

		//send
        if( send_chunk_len( buf_size, desc->fd ) != 0 ) {
        	return false;
        }
        log_debug_bytes("Chunk flush:", desc->buf, buf_size);
		ret = send(desc->fd, desc->buf, buf_size, 0) == buf_size;
	}
	return ret;
}
static bool write_buffered_callback_sha(pb_ostream_t *stream, const uint8_t * inbuf, size_t count) {
	ostream_buffered_desc_t * desc = (ostream_buffered_desc_t *) stream->state;
	assert( count < INT_MAX ); //make sure it fits in signed int
	int c = count;
	desc->bytes_that_should_have_been_written += c;

	if ((desc->buf_pos + count ) >= desc->buf_size) {
		/* Will I exceed the buffer size? then send buffer */
		while ((desc->buf_pos + c) >= desc->buf_size) {
			//copy over
			memcpy(desc->buf + desc->buf_pos, inbuf,
					desc->buf_size - desc->buf_pos);
			//encrypt
			SHA1_Update(desc->ctx, desc->buf, desc->buf_size);

			//send
			if (send_chunk_len(desc->buf_size, desc->fd) != 0) {
				return false;
			}

	        log_debug_bytes("Chunk:", desc->buf, desc->buf_size);
			if(!send(desc->fd, desc->buf, desc->buf_size, 0)
					== desc->buf_size ) { return false; }

			desc->bytes_written += desc->buf_size;
			c -= desc->buf_size - desc->buf_pos;
			inbuf += desc->buf_size - desc->buf_pos;
			desc->buf_pos = 0;
		}
		if( c > 0 ) {
			//copy to our buffer
			memcpy(desc->buf, inbuf, c);
			desc->buf_pos += c;
		}
	} else {
		//copy to our buffer
		memcpy(desc->buf + desc->buf_pos, inbuf, count);
		desc->buf_pos += count;
	}
    return true;
}
#if 0
static bool read_callback_sha(pb_istream_t *stream, uint8_t *buf, size_t count) {
    int fd = (intptr_t) stream->state;
    int result,i;

    result = recv(fd, buf, count, 0);

    SHA1_Update(&sha1ctx, buf, count);

    for (i = 0; i < count; ++i) {
        LOGI("%x", buf);
    }

    if (result == 0)
        stream->bytes_left = 0; /* EOF */

    return result == count;
}

pb_istream_t pb_istream_from_sha_socket(int fd) {
    pb_istream_t stream = { &read_callback_sha, (void*) (intptr_t) fd, SIZE_MAX };
    return stream;
}
#endif


//WARNING not re-entrant! Only 1 of these can be going at a time!
pb_ostream_t pb_ostream_from_sha_socket(ostream_buffered_desc_t * desc) {

    //pb_ostream_t stream = { write_callback_sha, (void*)desc, SIZE_MAX, 0 };
    pb_ostream_t stream = { write_buffered_callback_sha, (void*)desc, SIZE_MAX, 0 };



    return stream;
}


#if 0
#include "fault.h"
void LOGIFaults() {
#define minval( a,b ) a < b ? a : b
#define BUF_SZ 600
    size_t message_length;

    faultInfo * info = (faultInfo*)SHUTDOWN_MEM;

    if (info->magic == SHUTDOWN_MAGIC) {
        char buffer[BUF_SZ];
        if (sock > 0) {
            message_length = sizeof(faultInfo);
            usnprintf(buffer, sizeof(buffer), "POST /in/morpheus/fault HTTP/1.1\r\n"
                    "Host: in.skeletor.com\r\n"
                    "Content-type: application/x-protobuf\r\n"
                    "Content-length: %d\r\n"
                    "\r\n", message_length);
            memcpy(buffer + strlen(buffer), info, sizeof(faultInfo));

            LOGI("sending faultdata\n\r\n\r");

            send(sock, buffer, strlen(buffer), 0);

            recv(sock, buffer, sizeof(buffer), 0);

            //LOGI("Reply is:\n\r\n\r");
            //buffer[127] = 0; //make sure it terminates..
            //LOGI("%s", buffer);

            info->magic = 0;
        }
    }
}
#endif
int stop_connection(int * sock) {
	if( *sock >= 0 ) {
		LOGI("closing sock %d\n", *sock);
		close(*sock);
		*sock = -1;
		//NWP requires some time to come to terms with the disconnect...
		vTaskDelay(1000);
	}
    return *sock;
}


void
SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    char *caName;

    if(SL_SOCKET_ASYNC_EVENT == pSock->Event)
    {
        DISP("[SocketEvent] %d event received on socket %d\r\n",
        		pSock->SocketAsyncEvent.SockAsyncData.Type,
        		pSock->SocketAsyncEvent.SockAsyncData.Sd);

        switch(pSock->SocketAsyncEvent.SockAsyncData.Type)
        {
        case SL_SSL_NOTIFICATION_WRONG_ROOT_CA:
             caName = (char *) pvPortMalloc(pSock->SocketAsyncEvent.SockAsyncData.Val + 1);
            if (caName == NULL)
                    {
            		DISP("Buffer allocation failed\n\r");
                           return;
                    }
            memcpy(caName,
                                 pSock->SocketAsyncEvent.SockAsyncData.pExtraInfo,
                                 pSock->SocketAsyncEvent.SockAsyncData.Val);

            caName[pSock->SocketAsyncEvent.SockAsyncData.Val] = 0;

            DISP("[SocketEvent] Used wrong CA to verify the peer. please use %s\r\n",caName);

            vPortFree (caName);

            break;
        default:
            break;
        }
    }

}


int start_connection(int * sock, char * host, security_type sec) {
    sockaddr sAddr;
    timeval tv;
    int rv;
    unsigned long ipaddr = 0;
    int sock_begin = *sock;

    while(!wifi_status_get(HAS_IP)) {
    	LOGD(".");
    	vTaskDelay(1000);
    }
    set_backup_dns();

    if (*sock < 0) {
        if( sec == SOCKET_SEC_SSL ) {
        	wait_for_time(WAIT_FOREVER);
			*sock = socket(AF_INET, SOCK_STREAM, SL_SEC_SOCKET);

			LOGI("SSL\n");
			#define SL_SSL_CA_CERT_FILE_NAME "/cert/digi.cer"
			// configure the socket as SSLV3.0
			// configure the socket as RSA with RC4 128 SHA
			// setup certificate
			unsigned char method = SL_SO_SEC_METHOD_TLSV1_2;
#ifdef USE_SHA2
			unsigned int cipher = SL_WLAN_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256;
#else
			unsigned int cipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
#endif
			if(	sl_SetSockOpt(*sock, SL_SOL_SOCKET, SL_SO_SECMETHOD, &method, sizeof(method) ) < 0 ||
				sl_SetSockOpt(*sock, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &cipher, sizeof(cipher)) < 0 ||
				sl_SetSockOpt(*sock, SL_SOL_SOCKET, \
									   SL_SO_SECURE_FILES_CA_FILE_NAME, \
									   SL_SSL_CA_CERT_FILE_NAME, \
									   strlen(SL_SSL_CA_CERT_FILE_NAME))  < 0  )
			{
			LOGI( "error setting ssl options\r\n" );
			ble_reply_wifi_status(wifi_connection_state_SSL_FAIL);
			}
			{
				char buf[8];
				LOGI("Setting ignore cert store... %d\n",
						sl_SetSockOpt(*sock, SL_SOL_SOCKET, SL_SO_SECURE_DISABLE_CERTIFICATE_STORE, buf, sizeof(buf) ));
			}

        } else {
			*sock = socket(AF_INET, SOCK_STREAM, SL_IPPROTO_TCP);
        }
    }

    if (*sock < 0) {
        LOGI("Socket create failed %d\n\r", *sock);
        return -1;
    }

    //connect it up
    //LOGI("Connecting \n\r\n\r");
    if (*sock >= 0 && sock_begin < 0) {
    	LOGI("connecting sock %d %d\n", *sock, sock_begin);

    	tv.tv_sec = 2;             // Seconds
    	tv.tv_usec = 0;           // Microseconds. 10000 microseconds resolution
    	setsockopt(*sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

        SlSockNonblocking_t enableOption;
        enableOption.NonBlockingEnabled = 1;
        sl_SetSockOpt(*sock,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption)); // Enable/disable nonblocking mode

		#if !LOCAL_TEST
			if (!(rv = gethostbyname((_i8*)host, strlen(host), &ipaddr, SL_AF_INET))) {
				 LOGI("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
						 host, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
					   SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
				ble_reply_wifi_status(wifi_connection_state_DNS_RESOLVED);
			} else {
				LOGI("failed to resolves addr rv %d\n", rv);
				ble_reply_wifi_status(wifi_connection_state_DNS_FAILED);
				return stop_connection(sock);
			}

			sAddr.sa_family = AF_INET;
			// the source port
			if( sec == SOCKET_SEC_SSL ) {
				sAddr.sa_data[0] = 1;    //port 443, ssl
				sAddr.sa_data[1] = 0xbb; //port 443, ssl
			} else {
				sAddr.sa_data[0] = 0;  //port 80, http
				sAddr.sa_data[1] = 80; //port 80, http
			}
			sAddr.sa_data[2] = (char) ((ipaddr >> 24) & 0xff);
			sAddr.sa_data[3] = (char) ((ipaddr >> 16) & 0xff);
			sAddr.sa_data[4] = (char) ((ipaddr >> 8) & 0xff);
			sAddr.sa_data[5] = (char) (ipaddr & 0xff);
		#else

			sAddr.sa_family = AF_INET;
			// the source port
			sAddr.sa_data[0] = 0;//0xf;
			sAddr.sa_data[1] = 80;//0xa0; //4k
			sAddr.sa_data[2] = (char) () & 0xff);
			sAddr.sa_data[3] = (char) () & 0xff);
			sAddr.sa_data[4] = (char) () & 0xff);
			sAddr.sa_data[5] = (char) () & 0xff);

		#endif
		do {
			rv = connect(*sock, &sAddr, sizeof(sAddr));
			LOGD("`");
			vTaskDelay(500);
		} while( rv == SL_ERROR_BSD_EALREADY );
		if( rv < 0 ) {
			ble_reply_socket_error(rv);
			LOGI("Could not connect %d\n\r\n\r", rv);
			return stop_connection(sock);
		}
    } else {
    	LOGI("using sock %d %d\n", *sock, sock_begin);
    }
 	ble_reply_wifi_status(wifi_connection_state_SOCKET_CONNECTED);
    return 0;
}

//takes the buffer and reads <return value> bytes, up to buffer size
typedef int(*audio_read_cb)(char * buffer, int buffer_size);

#if 0
//buffer needs to be at least 128 bytes...
int send_audio_wifi(char * buffer, int buffer_size, audio_read_cb arcb) {
    int send_length;
    int rv = 0;
    int message_length;
    unsigned char mac[6];
    unsigned char mac_len = 6;
#if FAKE_MAC
    mac[0] = 0xab;
    mac[1] = 0xcd;
    mac[2] = 0xab;
    mac[3] = 0xcd;
    mac[4] = 0xab;
    mac[5] = 0xcd;
#else
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
#endif

    message_length = 110000;

    usnprintf(buffer, buffer_size, "POST /audio/%x%x%x%x%x%x HTTP/1.1\r\n"
            "Host: dev-in.hello.com\r\n"
            "Content-type: application/octet-stream\r\n"
            "Content-length: %d\r\n"
            "\r\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], message_length);
    send_length = strlen(buffer);

    LOGI("%s\n\r\n\r", buffer);

    //setup the connection
    if( start_connection() < 0 ) {
        LOGI("failed to start connection\n\r\n\r");
        return -1;
    }

    //LOGI("Sending request\n\r%s\n\r", buffer);
    rv = send(sock, buffer, send_length, 0);
    if ( rv <= 0) {
        LOGI("send error %d\n\r\n\r", rv);
        return stop_connection();
    }
    LOGI("sent %d\n\r\n\r", rv);

    while( message_length > 0 ) {
        int buffer_sent, buffer_read;
        buffer_sent = buffer_read = arcb( buffer, buffer_size );
        LOGI("read %d\n\r", buffer_read);

        while( buffer_read > 0 ) {
            send_length = minval(buffer_size, buffer_read);
            LOGI("attempting to send %d\n\r", send_length );

            rv = send(sock, buffer, send_length, 0);
            if (rv <= 0) {
                LOGI("send error %d\n\r", rv);
                return stop_connection();
            }
            LOGI("sent %d, %d left in buffer\n\r", rv, buffer_read);
            buffer_read -= rv;
        }
        message_length -= buffer_sent;
        LOGI("sent buffer, %d left\n\r\n\r", message_length);
    }

    memset(buffer, 0, buffer_size);

    //LOGI("Waiting for reply\n\r\n\r");
    rv = recv(sock, buffer, buffer_size, 0);
    if (rv <= 0) {
        LOGI("recv error %d\n\r\n\r", rv);
        return stop_connection();
    }
    LOGI("recv %d\n\r\n\r", rv);

    //LOGI("Reply is:\n\r\n\r");
    //buffer[127] = 0; //make sure it terminates..
    //LOGI("%s", buffer);


    return 0;
}
#endif

#if 1
#define SIG_SIZE 32
#include "sync_response.pb.h"


static bool decode_rx_data_pb(const uint8_t * buffer, uint32_t buffer_size, const pb_field_t fields[], void * structdata) {
	AES_CTX aesctx;
	SHA1_CTX sha1ctx;

	unsigned char * buf_pos = (unsigned char*)buffer;
	unsigned char sig[SIG_SIZE] = {0};
	unsigned char sig_test[SIG_SIZE] = {0};
	int i;
	int status;
	pb_istream_t stream;

	//memset( aesctx.iv, 0, sizeof( aesctx.iv ) );
	for (i = 0; i < AES_IV_SIZE; ++i) {
		aesctx.iv[i] = *buf_pos++;
		if (buf_pos > (buffer + buffer_size)) {
			LOGI("No IV\n");
			return false;
		}
	}
	for (i = 0; i < SIG_SIZE; ++i) {
		sig[i] = *buf_pos++;
		if (buf_pos > (buffer + buffer_size)) {
			LOGI("No SIG\n");
			return false;
		}
	}
	buffer_size -= (SIG_SIZE + AES_IV_SIZE);

	AES_set_key(&aesctx, aes_key, aesctx.iv, AES_MODE_128); //TODO: real key
	AES_convert_key(&aesctx);
	AES_cbc_decrypt(&aesctx, sig, sig, SIG_SIZE);

	SHA1_Init(&sha1ctx);
	SHA1_Update(&sha1ctx, buf_pos, buffer_size);
	//now verify sig
	SHA1_Final(sig_test, &sha1ctx);
	if (memcmp(sig, sig_test, SHA1_SIZE) != 0) {
		LOGI("signatures do not match\n");
		LOGI("%d %x %x\n", buffer_size, buf_pos, buffer );
#if 0
		for (i = 0; i < SHA1_SIZE; ++i) {
			LOGI("%02x", sig[i]);
		}
		LOGI("\nvs\n");
		for (i = 0; i < SHA1_SIZE; ++i) {
			LOGI("%02x", sig_test[i]);
		}
		LOGI("\n");
#endif
		return false;
	}

	/* Create a stream that will read from our buffer. */
	stream = pb_istream_from_buffer(buf_pos, buffer_size);
	/* Now we are ready to decode the message! */

	LOGI("data:\n");
	status = pb_decode(&stream,fields,structdata);

	/* Then just check for any errors.. */
	if (!status) {
		LOGI("Decoding error: %s\n", PB_GET_ERROR(&stream));
		return false;
	}

	return true;
}


#endif


int matchhere(char *regexp, char *text);
/* matchstar: search for c*regexp at beginning of text */
int matchstar(int c, char *regexp, char *text)
{
    do {    /* a * matches zero or more instances */
        if (matchhere(regexp, text))
            return 1;
    } while (*text != '\n' && (*text++ == c || c == '.'));
    return 0;
}

/* matchhere: search for regexp at beginning of text */
int matchhere(char *regexp, char *text)
{
    if (regexp[0] == '\0')
        return 1;
    if (regexp[1] == '*')
        return matchstar(regexp[0], regexp+2, text);
    if (regexp[0] == '$' && regexp[1] == '\0')
        return *text == '\n';
    if (*text!='\n' && (regexp[0]=='.' || regexp[0]==*text))
        return matchhere(regexp+1, text+1);
    return 0;
}


/* match: search for regexp anywhere in text */
int match(char *regexp, char *text)
{
    if (regexp[0] == '^')
        return matchhere(regexp+1, text);
    do {    /* must look even if string is empty */
        if (matchhere(regexp, text))
            return 1;
    } while (*text++ != '\n');
    return 0;
}
const char * get_top_version(void);


bool encode_pill_id(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	char* str = *arg;
	if(!str)
	{
		LOGI("encode_pill_id: No data to encode\n");
		return 0;
	}

	if (!pb_encode_tag_for_field(stream, field)){
		LOGI("encode_pill_id: Fail to encode tag\n");
		return 0;
	}

	return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

int http_response_ok( char* response_buffer)
{
	char* first_line = strstr(response_buffer, "\r\n") + 2;
	uint16_t first_line_len = first_line - response_buffer;
	assert(!( first_line && first_line_len > SERVER_REPLY_BUFSZ || first_line_len > strlen(response_buffer ) ));
	first_line = pvPortMalloc(first_line_len + 1);
	assert(first_line);

	memset(first_line, 0, first_line_len + 1);
	memcpy(first_line, response_buffer, first_line_len);
	LOGI("status: %s\n", first_line);
	wifi_status_set(UPLOADING, false);
	ble_reply_http_status(first_line);

	int resp_ok = match("2..", first_line);
	vPortFree(first_line);
	return resp_ok ? 0 : -1;
}
static char lcase(char a ) {
	return (a >= 'A' && a <= 'Z' ) ? a + ('a'-'A') : a;
}
static void lcasestr(char *s){
    while(*s) {
    	*s = lcase(*s);
    	++s;
    }
}

static bool validate_signatures( char * buffer, int sz, const pb_field_t fields[], void * structdata) {

    // Parse the response
    //LOGI("Reply is:\n\r%s\n\r", buffer);

    const char* header_content_len = "content-length: ";
    char * content = strstr(buffer, "\r\n\r\n") + 4;

    *(content-2) = 0;
    lcasestr(buffer);

    char * len_str = strstr(buffer, header_content_len) + strlen(header_content_len);

    LOGD( "Headers:\n%s", buffer );

    if (http_response_ok(buffer) != 0) {
    	wifi_status_set(UPLOADING, true);
        LOGI("Invalid response, endpoint return failure.\n");
        return false;
    }

    if( strstr(buffer, "no content") ) {
    	return true;
    }

    if (len_str == NULL) {
    	wifi_status_set(UPLOADING, true);
        LOGI("Failed to find Content-Length header\n");
        return false;
    }
    int len = atoi(len_str);
    if( len + (content - buffer) > sz ) {
    	LOGE("Content length %d exceeds rx buffer %d! %x %x\n", len, sz, content, buffer);
    	return false;
    }

    return decode_rx_data_pb((unsigned char*) content, len, fields, structdata);
}

//buffer needs to be at least 128 bytes...
int send_data_pb( char* host, const char* path, char ** recv_buf_ptr,
		uint32_t * recv_buf_size_ptr, const pb_field_t fields[],  void * structdata,
		protobuf_reply_callbacks * pb_cb, int * sock , security_type sec ) {
    int send_length = 0;
    int rv = 0;
    uint8_t sig[32]={0};
    bool status;

    uint32_t recv_buf_size = *recv_buf_size_ptr;
    char * recv_buf = *recv_buf_ptr;

    if (!recv_buf) {
    	LOGI("send_data_pb_callback needs a buffer\r\n");
    	goto failure;
    }
    {
    char hex_device_id[DEVICE_ID_SZ * 2 + 1] = {0};
    if(!get_device_id(hex_device_id, sizeof(hex_device_id)))
    {
        LOGE("get_device_id failed\n");
        goto failure;
    }

    //setup the connection
    if( start_connection(sock, host, sec) < 0 ) {
        LOGI("failed to start connection\n\r\n\r");
        goto failure;
    }

    if( sec == SOCKET_SEC_SSL ) {
		//check that it's still secure...
		rv = recv(*sock, recv_buf, SERVER_REPLY_BUFSZ, 0);
		if (rv != EAGAIN ) {
			LOGI("start recv error %d\n\r\n\r", rv);
			ble_reply_socket_error(rv);
			goto failure;
		}
    }

    usnprintf(recv_buf, recv_buf_size, "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-type: application/x-protobuf\r\n"
            "X-Hello-Sense-Id: %s\r\n"
    		"X-Hello-Sense-MFW: %x\r\n"
    		"X-Hello-Sense-TFW: %s\r\n"
    		"X-Hello-Sense-HW: %d\r\n"
            "Transfer-Encoding: chunked\r\n",
            path, host, hex_device_id, KIT_VER, get_top_version(), get_hw_ver());

    send_length = strlen(recv_buf);

    LOGD("Sending request\n\r%s\n\r", recv_buf);
    rv = send(*sock, recv_buf, send_length, 0);
    if (rv <= 0) {
        LOGI("send error %d\n\r\n\r", rv);
        ble_reply_socket_error(rv);
        goto failure;
    }

    }
    LOGD("HTTP header sent %d\n\r", rv);

    {
	ostream_buffered_desc_t desc;

	SHA1_CTX ctx;
	AES_CTX aesctx;
	pb_ostream_t stream = {0};
	int i, retries;

	memset(&desc,0,sizeof(desc));
	memset(&ctx,0,sizeof(ctx));
	memset(&aesctx,0,sizeof(aesctx));

	desc.buf = (uint8_t *)recv_buf;
	desc.inbuf_offset = 0;
	desc.buf_size = MAX_SHA256_SEND_SIZE;
	desc.ctx = &ctx;
	desc.fd = *sock;

	SHA1_Init(&ctx);

	/* Create a stream that will write to our buffer. */
	stream = pb_ostream_from_sha_socket(&desc);

	/* Now we are ready to encode the message! Let's go encode. */
	LOGI("data\n");
	status = pb_encode(&stream,fields,structdata);

	if( !flush_out_buffer(&desc) ) {
		LOGI("Flush failed\n");
        goto failure;
	}
	LOGI("a\n");

	/* sanity checks  */
	if (desc.bytes_written != desc.bytes_that_should_have_been_written) {
		LOGI("ERROR only %d of %d bytes written\r\n",desc.bytes_written,desc.bytes_that_should_have_been_written);
	}

	/* Then just check for any errors.. */
	if (!status) {
		LOGI("Encoding failed: %s\n", PB_GET_ERROR(&stream));
        goto failure;
	}

	//now sign it
	SHA1_Final(sig, &ctx);

	/*  fill in rest of signature with random numbers (eh?) */
	for (i = SHA1_SIZE; i < sizeof(sig); ++i) {
		sig[i] = (uint8_t)rand();
	}

    log_debug_bytes("SHA:", sig, sizeof(sig));
	//memset( aesctx.iv, 0, sizeof( aesctx.iv ) );

	/*  create AES initialization vector */
    for (i = 0; i < sizeof(aesctx.iv); ++i) {
		aesctx.iv[i] = (uint8_t)rand();
	}

	/*  send AES initialization vector */
	if( send_chunk_len( AES_IV_SIZE, *sock) != 0 ) {
        goto failure;
	}
    log_debug_bytes("IV:", aesctx.iv, sizeof(aesctx.iv));
	rv = send(*sock, aesctx.iv, AES_IV_SIZE, 0);

	if (rv != AES_IV_SIZE) {
		LOGI("Sending IV failed: %d\n", rv);
        goto failure;
	}

	AES_set_key(&aesctx, aes_key, aesctx.iv, AES_MODE_128); //todo real key
	AES_cbc_encrypt(&aesctx, sig, sig, sizeof(sig));


	/* send signature */
	if( send_chunk_len( sizeof(sig), *sock) != 0 ) {
        goto failure;
	}
    log_debug_bytes("sig:", sig, sizeof(sig));
	rv = send(*sock, sig, sizeof(sig), 0);

	if (rv != sizeof(sig)) {
		LOGI("Sending SHA failed: %d\n", rv);
		goto failure;
	}

    if( send_chunk_len(0, *sock) != 0 ) {
        goto failure;
    }
    ble_reply_wifi_status(wifi_connection_state_REQUEST_SENT);

    memset(recv_buf, 0, recv_buf_size);

    retries = 0;
    //keep looping while our socket error code is telling us to try again
    do {
		vTaskDelay(1000);
    	rv = recv(*sock, recv_buf+recv_buf_size-SERVER_REPLY_BUFSZ, SERVER_REPLY_BUFSZ, 0);
    	MAP_WatchdogIntClear(WDT_BASE); //clear wdt, it seems the SL_SPAWN hogs CPU here
        LOGD("x");
        if( rv == SERVER_REPLY_BUFSZ ) {
             recv_buf_size += SERVER_REPLY_BUFSZ;
             if( recv_buf_size > 10*1024 ) {
                 LOGI("error response too bug\n");
                 goto failure;
             }
    		 recv_buf = pvPortRealloc( recv_buf, recv_buf_size );
    		 assert(recv_buf);
    		 memset( recv_buf+recv_buf_size-SERVER_REPLY_BUFSZ, 0, SERVER_REPLY_BUFSZ);
    		 *recv_buf_ptr = recv_buf;
    		 *recv_buf_size_ptr = recv_buf_size;
    		 rv = EAGAIN;
    	}
    } while (rv == EAGAIN && retries++ < 70 ); // long poll endpoint times out at 60 seconds, so we need to wait a bit longer than that
    LOGI("rv %d\n", rv);

    if (rv <= 0) {
        LOGI("recv error %d\n\r\n\r", rv);
        ble_reply_socket_error(rv);
        goto failure;
    }
    }
    {
    pb_field_t * reply_fields = NULL;
    void * reply_structdata = NULL;

    if( pb_cb ) {
		if( pb_cb->get_reply_pb ) {
			pb_cb->get_reply_pb( &reply_fields, &reply_structdata );
			assert(reply_structdata);
		}
		if( reply_structdata && validate_signatures((char*)recv_buf, rv, reply_fields, reply_structdata ) ) {
			if( pb_cb && pb_cb->on_pb_success ) {
				pb_cb->on_pb_success( reply_structdata );
			}
			if( pb_cb && pb_cb->free_reply_pb ) {
				pb_cb->free_reply_pb( reply_structdata );
			}
			return 0;
		} else {
			if( pb_cb && pb_cb->on_pb_failure ) {
				pb_cb->on_pb_failure();
			}
		}
		if( reply_structdata && pb_cb->free_reply_pb ) {
			pb_cb->free_reply_pb( reply_structdata );
		}
    } else {
    	return http_response_ok((char*)recv_buf); //<- todo make non-200 from endpoints we don't have callbacks for close the socket
    }
    return stop_connection(sock);
    }

	failure:

	if( pb_cb && pb_cb->on_pb_failure ) {
		pb_cb->on_pb_failure();
	}
	return stop_connection(sock);
}


#include "ble_cmd.h"
#if 0
static bool _encode_encrypted_pilldata(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const array_data* array_holder = (array_data*)(*arg);
    if(!array_holder)
    {
    	LOGI("_encode_encrypted_pilldata: No data to encode\n");
        return false;
    }

    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag))
    {
    	LOGI("_encode_encrypted_pilldata: Fail to encode tag\n");
        return false;
    }

    return pb_encode_string(stream, array_holder->buffer, array_holder->length);
}
#endif

bool get_mac(unsigned char mac[6]) {
	int32_t ret;
	unsigned short mac_len = 6;

	ret = sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET, NULL, &mac_len, mac);

    if(ret != 0 && ret != SL_ESMALLBUF)
    {
    	LOGI("Fail to get MAC addr, err %d\n", ret);
        return false;  // If get mac failed, don't encode that field
    }
    return ret;
}

bool get_device_id(char * hex_device_id,uint32_t size_of_device_id_buffer) {
    uint8_t i = 0;

	if (size_of_device_id_buffer < DEVICE_ID_SZ * 2 + 1) {
		return false;
	}

    load_device_id();
	memset(hex_device_id, 0, size_of_device_id_buffer);

	for(i = 0; i < DEVICE_ID_SZ; i++){
		usnprintf(&hex_device_id[i * 2], 3, "%02X", device_id[i]);
	}

	return true;
}

bool _on_file_download(pb_istream_t *stream, const pb_field_t *field, void **arg);

void set_alarm( SyncResponse_Alarm * received_alarm, const char * ack, size_t ack_size );

static void _on_alarm_received( SyncResponse_Alarm* received_alarm, const char * ack, size_t ack_size)
{
	set_alarm( received_alarm, ack, ack_size );
}

static void _on_factory_reset_received()
{
    Cmd_factory_reset(0, 0);
}
#include "led_animations.h"

int force_data_push();

#ifdef SELF_PROVISION_SUPPORT
extern volatile bool provisioning_mode;
#endif

void boot_commit_ota();

static void _on_key_check_success(void * structdata){
	LOGF("signature validated\r\n");

#ifdef SELF_PROVISION_SUPPORT
	if (provisioning_mode) {
		sl_FsDel((unsigned char*)PROVISION_FILE, 0);
		wifi_reset();
		//green!
		play_led_wheel( LED_MAX, 0, LED_MAX, 0, 3600, 33,1);
		while (1) {
			vTaskDelay(100);
		}
	}

	provisioning_mode = false;
#endif
}
static void _on_key_check_failure( void * structdata){
	LOGF("signature validation fail\r\n");

#ifdef SELF_PROVISION_SUPPORT
	//light up if we're in provisioning mode but not if we're testing the key from top
	//this handles the overlap case where we want to get keys from top but the server doesn't yet have the blobs
	//allowing us to fall back to the key handshake with the server
	if (provisioning_mode && has_default_key()) {
		//red!
		play_led_wheel( LED_MAX, LED_MAX, 0, 0, 3600, 33,1);
		while (1) {
			vTaskDelay(100);
		}
	}
#endif
	save_aes_in_memory(DEFAULT_KEY);
	force_data_push(); //and make sure the retry happens right away
}

static void _key_check_reply(const NetworkResponse_t * response,
		char * reply_buf, int reply_sz, void * context) {
    vPortFree(context);
}
extern volatile bool use_dev_server;
char * get_server(void){
	if(use_dev_server){
		return DEV_DATA_SERVER;
	}
	return PROD_DATA_SERVER;
}
char * get_ws_server(void){
	if(use_dev_server){
		return DEV_WS_SERVER;
	}
	return PROD_WS_SERVER;
}
char * get_speech_server(void){
	if(use_dev_server){
		return DEV_SPEECH_SERVER;
	}
	return PROD_SPEECH_SERVER;
}
void on_key(uint8_t * key) {
	save_aes(key);
	load_aes();

	MorpheusCommand * test_command = (MorpheusCommand*)pvPortMalloc(sizeof(MorpheusCommand));
	memset(test_command, 0, sizeof(MorpheusCommand));
	test_command->type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE;
	test_command->version = PROTOBUF_VERSION;

    protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = ble_proto_get_morpheus_command;
    pb_cb.free_reply_pb = ble_proto_free_morpheus_command;
    pb_cb.on_pb_failure = _on_key_check_failure;
    pb_cb.on_pb_success = _on_key_check_success;

	NetworkTask_SendProtobuf(false, DATA_SERVER, CHECK_KEY_ENDPOINT,
			MorpheusCommand_fields, test_command, INT_MAX, _key_check_reply,
			test_command, &pb_cb, true );
}

static void _set_led_color_based_on_room_conditions(const SyncResponse* response_protobuf)
{
    if(response_protobuf->has_room_conditions)
    {
    	LOGI("lightson %d\n", response_protobuf->room_conditions);
    	switch(response_protobuf->room_conditions)
    	{
			case SyncResponse_RoomConditions_IDEAL:
				led_set_user_color(0x00, LED_MAX, 0x00,true);
			break;
			case SyncResponse_RoomConditions_WARNING:
				led_set_user_color(LED_MAX, LED_MAX, 0x00,true);
			break;
			case SyncResponse_RoomConditions_ALERT:
				led_set_user_color(253,45,8,true);
			break;
			default:
				led_set_user_color(0x00, 0x00, LED_MAX,true);
			break;
    	}
    }
    if(response_protobuf->has_room_conditions_lights_off)
    {
    	LOGI("lightsoff %d\n", response_protobuf->room_conditions_lights_off);
    	switch(response_protobuf->room_conditions_lights_off)
    	{
			case SyncResponse_RoomConditions_IDEAL:
				led_set_user_color(0x00, LED_MAX, 0x00,false);
			break;
			case SyncResponse_RoomConditions_WARNING:
				led_set_user_color(LED_MAX, LED_MAX, 0x00,false);
			break;
			case SyncResponse_RoomConditions_ALERT:
				led_set_user_color(253,45,8,false);
			break;
			default:
				led_set_user_color(0x00, 0x00, LED_MAX,false);
			break;
    	}
    }
}
void reset_to_factory_fw();

extern int data_queue_batch_size;
extern int pill_queue_batch_size;
static void _on_response_protobuf( SyncResponse* response_protobuf)
{
	bool enabled_audio_features_upload;

    if (response_protobuf->has_alarm) 
    {
    	if(response_protobuf->has_ring_time_ack){
    		_on_alarm_received(&response_protobuf->alarm, response_protobuf->ring_time_ack, sizeof(response_protobuf->ring_time_ack));

    	}else{
    		_on_alarm_received(&response_protobuf->alarm, NULL, 0);
    	}

    }

    if (response_protobuf->has_mac)
    {
        sl_NetCfgSet(SL_NETCFG_MAC_ADDRESS_SET,1,SL_MAC_ADDR_LEN,response_protobuf->mac.bytes);
        nwp_reset();
    }

    if(response_protobuf->has_reset_device && response_protobuf->reset_device)
    {
        LOGI("Server factory reset.\n");
        
        _on_factory_reset_received();
    }

    if(response_protobuf->has_reset_to_factory_fw && response_protobuf->reset_to_factory_fw) {
    	reset_to_factory_fw();
    	mcu_reset();
    }

    enabled_audio_features_upload = false;
    if (response_protobuf->has_audio_features_control) {
    	DISP("has_audio_features_control=TRUE\r\n");
    	if (response_protobuf->audio_features_control.has_enable_keyword_features && response_protobuf->audio_features_control.enable_keyword_features) {
    		enabled_audio_features_upload = true;
        	DISP("enabled_audio_features_upload=TRUE\r\n");
    	}
    	else {
        	DISP("enabled_audio_features_upload=FALSE\r\n");
    	}
    }
    else {
    	DISP("has_audio_features_control=FALSE\r\n");
    }
    audio_features_upload_set_upload_status(enabled_audio_features_upload);

    if( response_protobuf->has_batch_size ) {
    	data_queue_batch_size = response_protobuf->batch_size;
    }
    if( response_protobuf->has_pill_batch_size ) {
    	pill_queue_batch_size = response_protobuf->pill_batch_size;
    }

    if(response_protobuf->has_upload_log_level) {
    	set_loglevel(response_protobuf->upload_log_level);
    }
    if(response_protobuf->has_reset_mcu) {
    	if(response_protobuf->reset_mcu) {
    		mcu_reset();
    	}
	}
    _set_led_color_based_on_room_conditions(response_protobuf);
}

static void _get_sync_response(pb_field_t ** fields, void ** structdata){
	*fields = (pb_field_t *)SyncResponse_fields;
	*structdata = pvPortMalloc(sizeof(SyncResponse));
	assert(*structdata);
	if( *structdata ) {
		SyncResponse * response_protobuf = *structdata;
		memset(response_protobuf, 0, sizeof(SyncResponse));
		response_protobuf->pill_settings.funcs.decode = on_pill_settings;
		response_protobuf->files.funcs.decode = _on_file_download;
	}
}
static void _free_sync_response(void * structdata){
	vPortFree( structdata );
}

static void _on_sync_response_success( void * structdata){
	LOGF("signature validation success\r\n");
	boot_commit_ota();
	_on_response_protobuf((SyncResponse*)structdata);
}
static void _on_sync_response_failure( ){
    LOGF("signature validation fail\r\n");
}
bool send_pill_data_generic(batched_pill_data * pill_data, const char * endpoint){
	 protobuf_reply_callbacks pb_cb;

	pb_cb.get_reply_pb = _get_sync_response;
	pb_cb.free_reply_pb = _free_sync_response;
	pb_cb.on_pb_success = _on_sync_response_success;
	pb_cb.on_pb_failure = _on_sync_response_failure;

	return NetworkTask_SendProtobuf(true, DATA_SERVER,
			endpoint, batched_pill_data_fields, pill_data, INT_MAX,
			NULL, NULL, &pb_cb, false);
}
bool send_periodic_data(batched_periodic_data* data, bool forced, int32_t to) {
    protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = _get_sync_response;
    pb_cb.free_reply_pb = _free_sync_response;
    pb_cb.on_pb_success = _on_sync_response_success;
    pb_cb.on_pb_failure = _on_sync_response_failure;

	return NetworkTask_SendProtobuf(true, DATA_SERVER,
			DATA_RECEIVE_ENDPOINT, batched_periodic_data_fields, data, to,
			NULL, NULL, &pb_cb, forced);
}

#ifdef SELF_PROVISION_SUPPORT
static void _get_provision_response(pb_field_t ** fields, void ** structdata){
	*fields = (pb_field_t *)MorpheusCommand_fields;
	*structdata = pvPortMalloc(sizeof(ProvisionResponse));
	assert(*structdata);
    memset(structdata, 0, sizeof(ProvisionResponse));
}
static void _free_provision_response(void * structdata){
	if( structdata ) {
		vPortFree( structdata );
	};
}

static void _on_provision_response_success(void * structdata){
	ProvisionResponse * response_protobuf = structdata;
	LOGF("signature validation success\r\n");

	LOGI("Decoding PR %d %d\n",
			response_protobuf->has_key,
			response_protobuf->has_retry );

    if( response_protobuf->has_key ) {
    	on_key(response_protobuf->key.bytes);
    } else if( response_protobuf->has_retry && response_protobuf->retry ) {
    	//reset to known state
    	save_aes_in_memory(DEFAULT_KEY);
    }
	wifi_status_set(UPLOADING, false);
}
static void _on_provision_response_failure(pb_field_t * fields, void * structdata){
    LOGF("signature validation fail\r\n");
    wifi_status_set(UPLOADING, true);

    //this is bad... the device key has gone out of sync with the server...
    //reset to default key...
    save_aes_in_memory(DEFAULT_KEY);
}

bool send_provision_request(ProvisionRequest* req) {
    protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = _get_provision_response;
    pb_cb.free_reply_pb = _free_provision_response;
    pb_cb.on_pb_failure = _on_provision_response_failure;
    pb_cb.on_pb_success = _on_provision_response_success;

	return NetworkTask_SendProtobuf(true, DATA_SERVER, PROVISION_ENDPOINT,
			ProvisionRequest_fields, req, INT_MAX, NULL,NULL, &pb_cb, true);
}
#endif


#include "fatfs_cmd.h"

int audio_read_fn (char * buffer, int buffer_size) {
    memset(buffer, 0xabcd, buffer_size);

    return buffer_size;
}
#if 0
int Cmd_audio_test(int argc, char *argv[]) {
    short audio[1024];
    send_audio_wifi( (char*)audio, sizeof(audio), audio_read_fn );
    return (0);
}
#endif
//radio test functions
#ifdef BUILD_RADIO_TEST
#define FRAME_SIZE		1500
typedef enum
{
    LONG_PREAMBLE_MODE = 0,
    SHORT_PREAMBLE_MODE = 1,
    OFDM_PREAMBLE_MODE = 2,
    N_MIXED_MODE_PREAMBLE_MODE = 3,
    GREENFIELD_PREAMBLE_MODE = 4,
    MAX_NUM_PREAMBLE = 0xff
}RadioPreamble_e;

typedef enum
{
    RADIO_TX_CONTINUOUS = 1,
    RADIO_TX_PACKETIZED = 2,
    RADIO_TX_CW = 3,
    MAX_RADIO_TX_MODE = 0xff
}RadioTxMode_e;

typedef enum
{
    ALL_0_PATTERN = 0,
    ALL_1_PATTERN = 1,
    INCREMENTAL_PATTERN = 2,
    DECREMENTAL_PATTERN = 3,
    PN9_PATTERN = 4,
    PN15_PATTERN = 5,
    PN23_PATTERN = 6,
    MAX_NUM_PATTERN = 0xff
}RadioDataPattern_e;

int rawSocket;
uint8_t CurrentTxMode;

#define RADIO_CMD_BUFF_SIZE_MAX 100
#define DEV_VER_LENGTH			136
#define WLAN_DEL_ALL_PROFILES	255
#define CW_LOW_TONE			-25
#define CW_HIGH_TONE			25
#define CW_STOP					128

/**************************** Definitions for template frame ****************************/
#define RATE RATE_1M
#define FRAME_TYPE 0xC8											/* QOS data */
#define FRAME_CONTROL 0x01										/* TO DS*/
#define DURATION 0x00,0x00
#define RECEIVE_ADDR 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
#define TRANSMITTER_ADDR 0x00, 0x11, 0x22, 0x33, 0x44, 0x55
#define DESTINATION_ADDR 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
#define FRAME_NUMBER   0x00, 0x00
#define QOS_CONTROL	0x00, 0x00

#define RA_OFFSET		4
#define TA_OFFSET		10
#define DA_OFFSET		16

uint8_t TemplateFrame[] = {
                   /*---- wlan header start -----*/
                   FRAME_TYPE,                          							/* version type and sub type */
                   FRAME_CONTROL,                       							/* Frame control flag*/
                   DURATION,                            							/* duration */
                   RECEIVE_ADDR,                        							/* Receiver Address */
                   TRANSMITTER_ADDR,                    						/* Transmitter Address */
                   DESTINATION_ADDR,                    						/* destination Address*/
                   FRAME_NUMBER,                        							/* frame number */
                   QOS_CONTROL											/* QoS control */
};

int Cmd_RadioStopRX(int argc, char*argv[])
{
	sl_WlanRxStatStop();

    vTaskDelay(30 / portTICK_RATE_MS);

	return sl_Close(rawSocket);
}

int RadioStartRX(int eChannel)
{
	struct SlTimeval_t timeval;
	uint8_t * DataFrame = (uint8_t*)pvPortMalloc( FRAME_SIZE );
	assert(DataFrame);

	timeval.tv_sec =  0;             // Seconds
	timeval.tv_usec = 20000;             // Microseconds. 10000 microseconds resoultion

	sl_WlanRxStatStart();

	rawSocket = sl_Socket(SL_AF_RF, SL_SOCK_RAW, eChannel);

	if (rawSocket < 0)
	{
		return -1;
	}

	sl_SetSockOpt(rawSocket,SL_SOL_SOCKET,SL_SO_RCVTIMEO, &timeval, sizeof(timeval));    // Enable receive timeout

	recv(rawSocket, DataFrame, 1470, 0);

	vPortFree( DataFrame );
	return 0;
}

int Cmd_RadioStartRX(int argc, char*argv[])
{
	int channel;
	if(argc!=2) {
		LOGI("startrx <channel>\n");
	}
	channel = atoi(argv[1]);
	return RadioStartRX(channel);
}

int RadioGetStats(unsigned int *validPackets, unsigned int *fcsPackets,unsigned int *plcpPackets, int16_t *avgRssiMgmt, int16_t  *avgRssiOther, uint16_t * pRssiHistogram, uint16_t * pRateHistogram)
{

	SlGetRxStatResponse_t rxStatResp;

	sl_WlanRxStatGet(&rxStatResp, 0);

	*validPackets = rxStatResp.ReceivedValidPacketsNumber;
	*fcsPackets = rxStatResp.ReceivedFcsErrorPacketsNumber;
	*plcpPackets = rxStatResp.ReceivedPlcpErrorPacketsNumber;
	*avgRssiMgmt = rxStatResp.AvarageMgMntRssi;
	*avgRssiOther = rxStatResp.AvarageDataCtrlRssi;

	memcpy(pRssiHistogram, rxStatResp.RssiHistogram, sizeof(unsigned short) * SIZE_OF_RSSI_HISTOGRAM);

	memcpy(pRateHistogram, rxStatResp.RateHistogram, sizeof(unsigned short) * NUM_OF_RATE_INDEXES);

    return 0;

}

int Cmd_RadioGetStats(int argc, char*argv[])
{
	unsigned int valid_packets, fcs_packets, plcp_packets;
	int16_t avg_rssi_mgmt, avg_rssi_other;
	uint16_t * rssi_histogram;
	uint16_t * rate_histogram;
	int i;

	rssi_histogram = (uint16_t*)pvPortMalloc(sizeof(unsigned short) * SIZE_OF_RSSI_HISTOGRAM);
	rate_histogram = (uint16_t*)pvPortMalloc(sizeof(unsigned short) * NUM_OF_RATE_INDEXES);
	assert(rssi_histogram);
	assert(rate_histogram);

	RadioGetStats(&valid_packets, &fcs_packets, &plcp_packets, &avg_rssi_mgmt, &avg_rssi_other, rssi_histogram, rate_histogram);

	LOGI( "valid_packets %d\n", valid_packets );
	LOGI( "fcs_packets %d\n", fcs_packets );
	LOGI( "plcp_packets %d\n", plcp_packets );
	LOGI( "avg_rssi_mgmt %d\n", avg_rssi_mgmt );
	LOGI( "acg_rssi_other %d\n", avg_rssi_other );

	LOGI("rssi histogram\n");
	for (i = 0; i < SIZE_OF_RSSI_HISTOGRAM; ++i) {
		LOGI("%d\n", rssi_histogram[i]);
	}
	LOGI("rate histogram\n");
	for (i = 0; i < NUM_OF_RATE_INDEXES; ++i) {
		LOGI("%d\n", rate_histogram[i]);
	}

	vPortFree(rssi_histogram);
	vPortFree(rate_histogram);
	return 0;
}

int RadioStopTX(RadioTxMode_e eTxMode)
{
	int retVal;

	if (RADIO_TX_PACKETIZED == eTxMode)
	{
		retVal = 0;
	}

	if (RADIO_TX_CW == eTxMode)
	{
		sl_Send(rawSocket, NULL, 0, CW_STOP);
		vTaskDelay(30 / portTICK_RATE_MS);
		retVal = sl_Close(rawSocket);
	}

	if (RADIO_TX_CONTINUOUS == eTxMode)
	{
		retVal = sl_Close(rawSocket);
	}

	return retVal;
//	radioSM = RADIO_IDLE;
//
//	return sl_Close(rawSocket);
}

/* Note: the followings are not yet supported:
1) Power level
2) Preamble type
3) CW	*/
//int32_t RadioStartTX(RadioTxMode_e eTxMode, RadioPowerLevel_e ePowerLevel, Channel_e eChannel, RateIndex_e eRate, RadioPreamble_e ePreamble, RadioDataPattern_e eDataPattern, uint16_t size, uint32_t delay, uint8_t * pDstMac)
int32_t RadioStartTX(RadioTxMode_e eTxMode, uint8_t powerLevel_Tone, int eChannel, SlRateIndex_e eRate, RadioPreamble_e ePreamble, RadioDataPattern_e eDataPattern, uint16_t size, uint32_t delay_amount, uint8_t overrideCCA, uint8_t * pDstMac)
{
	uint16_t loopIdx;
	int32_t length;
	uint32_t numberOfFrames = delay_amount;
	uint8_t pConfigLen = SL_BSSID_LENGTH;
	CurrentTxMode = (uint8_t) eTxMode;
	int32_t minDelay;
	uint8_t * DataFrame = (uint8_t*)pvPortMalloc( FRAME_SIZE );
	assert(DataFrame);

	if ((RADIO_TX_PACKETIZED == eTxMode) || (RADIO_TX_CONTINUOUS == eTxMode))
	{
		/* build the frame */
		switch (eDataPattern)
		{
			case ALL_0_PATTERN:
				memset(DataFrame, 0, sizeof(DataFrame));

				break;
	    		case ALL_1_PATTERN:
				memset(DataFrame, 1, sizeof(DataFrame));

				break;
			case INCREMENTAL_PATTERN:
				for (loopIdx = 0; loopIdx < FRAME_SIZE; loopIdx++)
					DataFrame[loopIdx] = (uint8_t)loopIdx;

				break;
	    		case DECREMENTAL_PATTERN:
				for (loopIdx = 0; loopIdx < FRAME_SIZE; loopIdx++)
					DataFrame[loopIdx] = (uint8_t)(FRAME_SIZE - 1 - loopIdx);

				break;
			default:
				memset(DataFrame, 0, sizeof(DataFrame));
		}

		sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL,&pConfigLen, &TemplateFrame[TA_OFFSET]);
		memcpy(&TemplateFrame[RA_OFFSET], pDstMac, SL_BSSID_LENGTH);
		memcpy(&TemplateFrame[DA_OFFSET], pDstMac, SL_BSSID_LENGTH);

		memcpy(DataFrame, TemplateFrame, sizeof(TemplateFrame));

		/* open a RAW/DGRAM socket based on CCA override*/
		if (overrideCCA == 1)
			rawSocket = sl_Socket(SL_AF_RF, SL_SOCK_RAW, eChannel);
		else
			rawSocket = sl_Socket(SL_AF_RF, SL_SOCK_DGRAM, eChannel);

		if (rawSocket < 0)
		{
			vPortFree( DataFrame );
			return -1;
		}
	}



	if (RADIO_TX_PACKETIZED == eTxMode)
	{
			length = size;
			portTickType xDelay;


			while ((length == size))
			{
                            /* transmit the frame */
							minDelay = (delay_amount%50);
                            length = sl_Send(rawSocket, DataFrame, size, SL_RAW_RF_TX_PARAMS(eChannel, eRate, powerLevel_Tone, ePreamble));
                            //UtilsDelay((delay_amount*CPU_CYCLES_1MSEC)/12);
                            xDelay= minDelay / portTICK_RATE_MS;
                            vTaskDelay(xDelay);

                			minDelay = (delay_amount - minDelay);
                			while(minDelay > 0)
                			{
                				vTaskDelay(50/portTICK_RATE_MS);
                				minDelay -= 50;
                			}
			}

			if (length != size)
			{
				RadioStopTX((RadioTxMode_e)CurrentTxMode);

				vPortFree( DataFrame );
				return -1;
			}

			if(sl_Close(rawSocket) < 0)
			{
				vPortFree( DataFrame );
				return -1;
			}

			vPortFree( DataFrame );
			return 0;
		}

	if (RADIO_TX_CONTINUOUS == eTxMode)
	{
		sl_SetSockOpt(rawSocket, SL_SOL_PHY_OPT, SL_SO_PHY_NUM_FRAMES_TO_TX, &numberOfFrames, sizeof(uint32_t));

		sl_Send(rawSocket, DataFrame, size, SL_RAW_RF_TX_PARAMS(eChannel, eRate, powerLevel_Tone, ePreamble));

	}

	if (RADIO_TX_CW == eTxMode)
	{
		rawSocket = sl_Socket(SL_AF_RF,SL_SOCK_RAW,eChannel);
		if(rawSocket < 0)
		{
			vPortFree( DataFrame );
			return -1;
		}

		sl_Send(rawSocket, NULL, 0, powerLevel_Tone);
	}

	vPortFree( DataFrame );
	return 0;
}

int Cmd_RadioStartTX(int argc, char*argv[])
{
	RadioTxMode_e mode;
	uint8_t pwrlvl;
	int chnl;
	SlRateIndex_e rate;
	RadioPreamble_e preamble;
	RadioDataPattern_e datapattern;
	uint16_t size;
	uint32_t delay;
	uint8_t overridecca;
	uint8_t dest_mac[6];
	int i;

	if(argc!=16) {
		LOGI("startx <mode, RADIO_TX_PACKETIZED=2, RADIO_TX_CW=3, RADIO_TX_CONTINUOUS=1> "
				          "<power level 0-15, as dB offset from max power so 0 high>"
						  "<channel> <rate 1=1M, 2=2M, 3=5.5M, 4=11M, 6=6M, 7=9M, 8=12M, 9=18M, 10=24M, 11=36M, 12=48M, 13=54M, 14 to 21 = MCS_0 to 7"
						  "<preamble, 0=long, 1=short, 2=odfm, 3=mixed, 4=greenfield>"
				          "<data pattern 0=all 0, 1=all 1, 2=incremental, 3=decremental, 4=PN9, 5=PN15, 6=PN23>"
				          "<size> <delay amount> <override CCA> <destination mac address, given as six 8 bit hex values>"
						);
		return -1;
	}
	mode = (RadioTxMode_e)atoi(argv[1]);
	pwrlvl = atoi(argv[2]);
	chnl = atoi(argv[3]);
	rate = (SlRateIndex_e)atoi(argv[4]);
	preamble = (RadioPreamble_e)atoi(argv[5]);
	datapattern = (RadioDataPattern_e)atoi(argv[6]);
	size = atoi(argv[7]);
	delay = atoi(argv[8]);
	overridecca = atoi(argv[9]);

	for (i = 0; i < 6; ++i) {
		dest_mac[i] = strtol(argv[10+i], NULL, 16);
	}
	return RadioStartTX(mode, pwrlvl, chnl, rate, preamble, datapattern, size, delay, overridecca, dest_mac);
}

int Cmd_RadioStopTX(int argc, char*argv[])
{
	RadioTxMode_e mode;
	if(argc!=2) {
		LOGI("stoptx <mode, RADIO_TX_PACKETIZED=2, RADIO_TX_CW=3, RADIO_TX_CONTINUOUS=1>\n");
	}
	mode = (RadioTxMode_e)atoi(argv[1]);
	return RadioStopTX(mode);
}
//end radio test functions
#endif

SlWlanSecParams_t make_sec_params(const char* ssid, const char* password, int sec_type, int version)
{
	SlWlanSecParams_t secParam = {0};

	if(sec_type == 3 ) {
		sec_type = 2;
	}
	secParam.Key = (signed char*)password;
	secParam.KeyLen = password == NULL ? 0 : strlen(password);
	secParam.Type = sec_type;

    if( version > 0 && secParam.Type == 1 ) {
    	uint8_t wep_hex[14];
    	int i;

		for(i=0;i<strlen((char*)secParam.Key)/2;++i) {
			char num[3] = {0};
			memcpy( num, secParam.Key+i*2, 2);
			wep_hex[i] = strtol( num, NULL, 16 );
		}
		secParam.KeyLen = i;
		wep_hex[i++] = 0;

		for(i=0;i<secParam.KeyLen;++i) {
			UARTprintf("%x:", wep_hex[i] );
		}
		UARTprintf("\n" );
		memcpy( secParam.Key, wep_hex, secParam.KeyLen + 1 );
    }
    return secParam;
}
int connect_wifi(const char* ssid, const char* password, int sec_type, int version, int * idx, bool save)
{
	static uint32_t priority = 0;
	int16_t ret = 0;
	SlWlanSecParams_t secParam = make_sec_params(ssid, password, sec_type, version);

	ret = sl_WlanConnect((_i8*) ssid, strlen(ssid), NULL, sec_type == SL_WLAN_SEC_TYPE_OPEN ? NULL : &secParam, 0);

	if( save ) {
		*idx = sl_WlanProfileAdd((_i8*) ssid, strlen(ssid), NULL,
							&secParam, NULL, ++priority, 0);
	}
	if(ret == 0 || ret == -71)
	{
		LOGI("WLAN connect attempt issued\n");
	    ble_reply_wifi_status(wifi_connection_state_WLAN_CONNECTING);
	    return 0;
	} else {
        return -1;
	}
}
static xSemaphoreHandle _sl_status_mutex;
static unsigned int _wifi_status;

void wifi_status_init()
{
    _sl_status_mutex = xSemaphoreCreateMutex();
    if(!_sl_status_mutex)
    {
        LOGI("Create _sl_status_mutex failed\n");
    }
    _wifi_status = 0;
}

int wifi_status_get(unsigned int status)
{
    xSemaphoreTake(_sl_status_mutex, portMAX_DELAY);
    int ret = _wifi_status & status;
    xSemaphoreGive(_sl_status_mutex);
    return ret;
}

int wifi_status_set(unsigned int status, int remove_status)
{
    if(xSemaphoreTake(_sl_status_mutex, portMAX_DELAY) != pdTRUE)
    {
        return _wifi_status;
    }

    if(remove_status)
    {
        _wifi_status &= ~status;
    }else{
        _wifi_status |= status;
    }
    int ret = _wifi_status;
    xSemaphoreGive(_sl_status_mutex);
    return ret;
}

#if defined(BUILD_SERVERS) && defined(BUILD_TELNET_SERVER)
#include "ctype.h"
#if 0
#define SVR_LOGI UARTprintf
#else
#define SVR_LOGI(...)
#endif

static void make_nonblocking( volatile int * sock ) {
	SlSockNonblocking_t enableOption;
	enableOption.NonblockingEnabled = 1;
	sl_SetSockOpt(*sock, SL_SOL_SOCKET, SL_SO_NONBLOCKING,
			(_u8 *)&enableOption, sizeof(enableOption));
}

//Server only works for ascii based protocols!
static void serv(int port, volatile int * connection_socket, int (*cb)(volatile int *, char*, int), const char * proc_on ) {
	sockaddr local_addr;
	sockaddr their_addr;
	socklen_t addr_size;
	timeval tv;

	local_addr.sa_family = SL_AF_INET;
	local_addr.sa_data[0] = ((port >> 8) & 0xFF);
	local_addr.sa_data[1] = (port & 0xFF);

	//all 0 => Own IP address
	memset(&local_addr.sa_data[2], 0, 4);

	int sock;
	while (1) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		tv.tv_sec = 1;             // Seconds
		tv.tv_usec = 0;           // Microseconds. 10000 microseconds resolution
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

		SVR_LOGI( "SVR: bind\n");
		if ( bind(sock, &local_addr, sizeof(local_addr)) < 0) {
			close(sock);
			sock = -1;
			continue;
		} SVR_LOGI( "SVR: listen\n");
		if ( listen(sock, 0) < 0) {
			close(sock);
			sock = -1;
			continue;
		}

		char * buf = pvPortMalloc(64);
		char * linebuf = pvPortMalloc(512);
		while (sock > 0) {
			new_connection:
			*connection_socket = -1;
			addr_size = sizeof(their_addr);
			SVR_LOGI( "SVR: accept\n");
			int inbufsz = 0;
			assert(buf && linebuf);
			memset(buf, 0, 64);
			memset(linebuf, 0, 512);

			*connection_socket = sl_AcceptNoneThreadSafe(sock, &their_addr, &addr_size);
			setsockopt(*connection_socket, SOL_SOCKET, SO_RCVTIMEO, &tv,
					sizeof(tv)); // Enable receive timeout

			make_nonblocking( connection_socket );

			SVR_LOGI( "SVR: connected %d %d\n", *connection_socket, sock );
			while (*connection_socket > 0) {
				SVR_LOGI( "SVR: recv\n");
				int sz = EAGAIN;

				memset(buf, 0, 64);
				while (sz == EAGAIN || sz == EWOULDBLOCK) {
					sz = recv(*connection_socket, buf, 64, 0);
					if ((sz <= 0 && sz != EAGAIN && sz != EWOULDBLOCK)
							|| *connection_socket <= 0) {
						SVR_LOGI( "SVR: client disconnect\n");
						close(*connection_socket);
						*connection_socket = -1;
						goto new_connection;
					}
				}

				SVR_LOGI( "SVR: got %d\n", sz);
				if (sz <= 0) {
					close(*connection_socket);
					*connection_socket = -1;
					goto new_connection;
				}

				char * ptr;
				for( ptr=buf; *ptr; ++ptr ) {
					if(!isprint(*ptr) && *ptr != '\r' && *ptr != '\n' ) {
						SVR_LOGI( "SVR: unprintable\n");
						memset(buf, 0, 64);
						sz = 0;
						continue;
					}
				}

				if (sz + inbufsz > 512 || strlen(buf) != sz ) {
					SVR_LOGI( "SVR: String lengths wrong\n");
					memset(linebuf, 0, 512);
					inbufsz = 0;
					continue;
				}
				memcpy(linebuf + inbufsz, buf, sz);
				inbufsz += sz;

				if ( strstr( linebuf, proc_on ) != 0 ) {
					if (cb(connection_socket, linebuf, inbufsz) < 0) {
						memset(linebuf, 0, 512);
						inbufsz = 0;
						goto new_connection;
					}
					memset(linebuf, 0, 512);
					inbufsz = 0;
				}
			}
		}

		SVR_LOGI( "SVR: lost sock\n");
		*connection_socket = -1;
		close(sock);
		sock = -1;
		vPortFree(buf);
		vPortFree(linebuf);
	}
}

static int send_buffer( volatile int* sock, const char * str, int len ) {
	int sent = 0;
	int retries = 0;
	while (sent < len) {
		int sz = send(*sock, str, len, 0);
		if ((sz <= 0 && sz != EAGAIN && sz != EWOULDBLOCK )
		 || (sz == EWOULDBLOCK && ++retries > 10)) {
			close(*sock);
			*sock = -1;
			return -1;
		}
		if( sz == EWOULDBLOCK ) {
			//vTaskDelay(100);
			return 0; //can't stop will interrupt calling thread!
		}
		sent += sz;
	}
	return sent;
}
#ifdef BUILD_TELNET_SERVER
volatile static int telnet_connection_sock;
void telnetPrint(const char * str, int len) {
	if ( telnet_connection_sock > 0 && wifi_status_get(HAS_IP)) {
		send_buffer( &telnet_connection_sock, str, len );
	}
}
void
CmdLineProcess(void * line);
static int cli_cb(volatile int *sock, char * linebuf, int inbufsz) {
	if (inbufsz > 2) {
		//
		// Pass the line from the user to the command processor.  It will be
		// parsed and valid commands executed.
		//
		char * args = pvPortMalloc(inbufsz + 1);
		memcpy(args, linebuf, inbufsz + 1);
		args[inbufsz - 2] = 0;
		xTaskCreate(CmdLineProcess, "commandTask", 5 * 1024 / 4, args, 4, NULL);
	}
	return 0;
}
void telnetServerTask(void *params) {
#define INTERPRETER_PORT 224
    serv( INTERPRETER_PORT, &telnet_connection_sock, cli_cb, "\n" );
}
#endif
#if 0 //used in proof of concept
static int echo_cb( volatile int * sock,  char * linebuf, int inbufsz ) {
	if ( send( *sock, "\n", 1, 0 ) <= 0 ) {
		close(*sock);
		*sock = -1;
		return -1;
	}
	if ( send_buffer( sock, linebuf, inbufsz ) <= 0 ) {
		return -1;
	}
	return 0;
}
#endif

static int send_buffer_chunked(volatile int * sock, const char * str, int len) {
	if (send_chunk_len( len, *sock ) < 0 ) {
		close(*sock);
		*sock = -1;
		return -1;
	}
	if (send_buffer(sock, str, len) <= 0) {
		return -1;
	}
	return 0;
}

extern  xSemaphoreHandle i2c_smphr;
int get_temp();
int get_light();
int get_prox();
int get_dust();
int get_humid();
static int http_cb(volatile int * sock, char * linebuf, int inbufsz) {
	const char * http_response = "HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Transfer-Encoding: chunked\r\n";
	const char * html_start = "<HTML>"
			"<HEAD>"
			"<TITLE>Hello</TITLE>"
			"<meta http-equiv=\"refresh\" content=\"60\">"
			"</HEAD>"
			"<BODY><H1>Sense Info</H1><P>";
	const char * html_end =
            "</P></BODY></HTML>";

	if( strstr(linebuf, "HEAD" ) != 0 ) {
		if (send_buffer(sock, http_response, strlen(http_response)) <= 0) {
			return -1;
		}
	}
	if( strstr(linebuf, "GET" ) != 0 ) {
		if( send_buffer( sock, http_response, strlen(http_response)) <= 0 ) {
			return -1;
		}
		if( send_buffer_chunked(sock, html_start, strlen(html_start)) < 0 ){
			return -1;
		}

		xSemaphoreTakeRecursive(i2c_smphr, portMAX_DELAY);
		char * html = pvPortMalloc(128);
		usnprintf( html, 128, "Temperature is %d<br>", get_temp());
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done_i2c;
		}
		usnprintf( html, 128, "Humidity is %d<br>", get_humid());
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done_i2c;
		}
		usnprintf( html, 128, "Light is %d<br>", get_light());
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done_i2c;
		}
		usnprintf(html, 128, "Proximity is %d<br>", get_prox());
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done_i2c;
		}
		done_i2c:
		xSemaphoreGiveRecursive(i2c_smphr);
		if( *sock < 0 ) {
			vPortFree(html);
			return -1;
		}

		usnprintf( html, 128, "Dust is %d<br>", get_dust());
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done;
		}
		usnprintf(html, 128, "Time is %d<br>", get_time());
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done;
		}
		usnprintf(html, 128, "Uptime is %d<br>", xTaskGetTickCount()/1000);
		if( send_buffer_chunked( sock, html, strlen(html)) < 0 ) {
			goto done;
		}
		if( send_buffer_chunked( sock, html_end, strlen(html_end)) < 0 ) {
			goto done;
		}
		done:
		vPortFree(html);
		if( *sock < 0 ) {
			return -1;
		}

		if (send_chunk_len( 0, *sock ) < 0) {
			close(*sock);
			*sock = -1;
			return -1;
		}
	}
#if 0
	if( strstr(linebuf, "Connection: keep-alive" ) != 0 ) {
		return 0;
	}
#endif
	close(*sock);
	return -1;
}

void httpServerTask(void *params) {
	volatile int http_sock = 0;
    serv( 80, &http_sock, http_cb, "\r\n\r\n" );
}


#endif
