#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "uartstdio.h"

#include "wlan.h"
#include "socket.h"
#include "simplelink.h"
#include "protocol.h"

#include "ota_api.h"

#include "wifi_cmd.h"
#include "networktask.h"

#define ROLE_INVALID (-5)

int sl_mode = ROLE_INVALID;

unsigned int sl_status = 0;

#include "rom_map.h"
#include "prcm.h"
#include "utils.h"

#include "hw_memmap.h"
#include "rom_map.h"
#include "gpio.h"

#define FAKE_MAC 0

xSemaphoreHandle alarm_smphr;
SyncResponse_Alarm alarm;

void mcu_reset()
{
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
void nwp_reset() {
    sl_WlanSetMode(ROLE_STA);
    sl_Stop(SL_STOP_TIMEOUT);
    sl_mode = sl_Start(NULL, NULL, NULL);
}

#include "ota_usr.h"


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
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    // Unused in this application
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
    //
    // This application doesn't work w/ socket - Events are not expected
    //
}

//****************************************************************************
//
//!    \brief This function handles WLAN events
//!
//! \param  pSlWlanEvent is the event passed to the handler
//!
//! \return None
//
//****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pSlWlanEvent) {
    switch (pSlWlanEvent->Event) {
#if 0 //todo bring this back after ti realises they've mucked it up
    case SL_WLAN_SMART_CONFIG_START_EVENT:
        /* SmartConfig operation finished */
        /*The new SSID that was acquired is: pWlanEventHandler->EventData.smartConfigStartResponse.ssid */
        /* We have the possiblity that also a private token was sent to the Host:
         *  if (pWlanEventHandler->EventData.smartConfigStartResponse.private_token_len)
         *    then the private token is populated: pWlanEventHandler->EventData.smartConfigStartResponse.private_token
         */
        UARTprintf("SL_WLAN_SMART_CONFIG_START_EVENT\n\r");
        break;
#endif
    case SL_WLAN_SMART_CONFIG_STOP_EVENT:
        UARTprintf("SL_WLAN_SMART_CONFIG_STOP_EVENT\n\r");
        break;
    case SL_WLAN_CONNECT_EVENT:
        UARTprintf("SL_WLAN_CONNECT_EVENT\n\r");
        sl_status |= CONNECT;
        sl_status &= ~CONNECTING;
        break;
    case SL_WLAN_DISCONNECT_EVENT:
        UARTprintf("SL_WLAN_DISCONNECT_EVENT\n\r");
        sl_status &= ~CONNECT;
        sl_status &= ~HAS_IP;
        break;
    default:
        break;
    }
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

int Cmd_led(int argc, char *argv[]);

void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {

    switch (pNetAppEvent->Event) {
	case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
	case SL_NETAPP_IPV6_IPACQUIRED_EVENT:
		UARTprintf("SL_NETAPP_IPV4_ACQUIRED\n\r");
		{
			int seed = (unsigned) PRCMSlowClkCtrGet();
			UARTprintf("seeding %d\r\n", seed);
			srand(seed); //seed with low bits of lf clock when connecting(not sure when it happens, gives some more entropy).
		}

		sl_status |= HAS_IP;

        Cmd_led(0,0);
		break;

	case SL_NETAPP_IP_LEASED_EVENT:
		sl_status |= IP_LEASED;
        break;
    default:
        break;
    }
}

void antsel(unsigned char a)
{
    if(a == 1)
    {
         MAP_GPIOPinWrite(GPIOA3_BASE, 0xC, 0x8);
    }
    else if(a == 2)
    {
        MAP_GPIOPinWrite(GPIOA3_BASE, 0xC, 0x4);
    }
    return;
}


int Cmd_antsel(int argc, char *argv[]) {
    if (argc != 2) {
        UARTprintf( "usage: antsel <1=IFA or 2=chip>\n\r");
        return -1;
    }
    antsel( *argv[1] ==  '1' ? 1 : 2 );

    return 0;
}

void wifi_reset()
{
    int16_t ret = sl_WlanProfileDel(0xFF);
    if(ret)
    {
        UARTprintf("Delete all stored endpoint failed, error %d.\n", ret);
    }else{
        UARTprintf("All stored WIFI EP removed.\n");
    }

    ret = sl_WlanDisconnect();
    if(ret == 0){
        UARTprintf("WIFI disconnected");
    }else{
        UARTprintf("Disconnect WIFI failed, error %d.\n", ret);
    }
}

int Cmd_disconnect(int argc, char *argv[]) {

    wifi_reset();
    return (0);
}
int Cmd_connect(int argc, char *argv[]) {
    SlSecParams_t secParams;

    if (argc != 4) {
        UARTprintf(
                "usage: connect <ssid> <key> <security: 0=open, 1=wep, 2=wpa>\n\r");
    }

    secParams.Key = (_i8*)argv[2];
    secParams.KeyLen = strlen(argv[2]);
    secParams.Type = atoi(argv[3]);

    sl_WlanConnect((_i8*)argv[1], strlen(argv[1]), NULL, &secParams, 0);
    sl_WlanProfileAdd((_i8*)argv[1], strlen(argv[1]), NULL, &secParams, NULL, 0, 0 );
    return (0);
}

int Cmd_status(int argc, char *argv[]) {
    unsigned char ucDHCP = 0;
    unsigned char len = sizeof(SlNetCfgIpV4Args_t);
    //
    // Get IP address
    //    unsigned char len = sizeof(_NetCfgIpV4Args_t);
    SlNetCfgIpV4Args_t ipv4 = { 0 };

    sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO, &ucDHCP, &len,
            (unsigned char *) &ipv4);
    //
    // Send the information
    //
    UARTprintf("%x ip 0x%x submask 0x%x gateway 0x%x dns 0x%x\n\r", sl_status,
            ipv4.ipV4, ipv4.ipV4Mask, ipv4.ipV4Gateway, ipv4.ipV4DnsServer);
    return 0;
}

// callback routine
void pingRes(SlPingReport_t* pUARTprintf) {
    // handle ping results
    UARTprintf(
            "Ping tx:%d rx:%d min time:%d max time:%d avg time:%d test time:%d\n",
            pUARTprintf->PacketsSent, pUARTprintf->PacketsReceived,
            pUARTprintf->MinRoundTime, pUARTprintf->MaxRoundTime, pUARTprintf->AvgRoundTime,
            pUARTprintf->TestTime);
}

int Cmd_ping(int argc, char *argv[]) {
    static SlPingReport_t UARTprintf;
    SlPingStartCommand_t pingCommand;

    pingCommand.Ip = SL_IPV4_VAL(192, 3, 116, 75); // destination IP address is my router's IP
    pingCommand.PingSize = 32;                     // size of ping, in bytes
    pingCommand.PingIntervalTime = 100;   // delay between pings, in miliseconds
    pingCommand.PingRequestTimeout = 1000; // timeout for every ping in miliseconds
    pingCommand.TotalNumberOfAttempts = 3; // max number of ping requests. 0 - forever
    pingCommand.Flags = 1;                        // UARTprintf after each ping

    sl_NetAppPingStart(&pingCommand, SL_AF_INET, &UARTprintf, pingRes);
    return (0);
}

unsigned long unix_time() {
    char buffer[48];
    int rv = 0;
    SlSockAddr_t sAddr;
    SlSockAddrIn_t sLocalAddr;
    int iAddrSize;
    unsigned long long ntp;
    unsigned long ipaddr;
    int sock;

    SlTimeval_t tv;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    tv.tv_sec = 2;             // Seconds
    tv.tv_usec = 0;             // Microseconds. 10000 microseconds resolution
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

    if (sock < 0) {
        UARTprintf("Socket create failed\n\r");
        return 0;
    }
    UARTprintf("Socket created\n\r");

//
    // Send a query ? to the NTP server to get the NTP time
    //
    memset(buffer, 0, sizeof(buffer));

#define NTP_SERVER "pool.ntp.org"
    if (!(rv = gethostbyname(NTP_SERVER, strlen(NTP_SERVER), &ipaddr, AF_INET))) {
        UARTprintf(
                "Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
                NTP_SERVER, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
                SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
    } else {
        UARTprintf("failed to resolve ntp addr rv %d\n", rv);
        close(sock);
        return 0;
    }

    sAddr.sa_family = AF_INET;
    // the source port
    sAddr.sa_data[0] = 0x00;
    sAddr.sa_data[1] = 0x7B;    // UDP port number for NTP is 123
    sAddr.sa_data[2] = (char) ((ipaddr >> 24) & 0xff);
    sAddr.sa_data[3] = (char) ((ipaddr >> 16) & 0xff);
    sAddr.sa_data[4] = (char) ((ipaddr >> 8) & 0xff);
    sAddr.sa_data[5] = (char) (ipaddr & 0xff);

    buffer[0] = 0b11100011;   // LI, Version, Mode
    buffer[1] = 0;     // Stratum, or type of clock
    buffer[2] = 6;     // Polling Interval
    buffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    buffer[12] = 49;
    buffer[13] = 0x4E;
    buffer[14] = 49;
    buffer[15] = 52;

    UARTprintf("Sending request\n\r\n\r");
    rv = sendto(sock, buffer, sizeof(buffer), 0, &sAddr, sizeof(sAddr));
    if (rv != sizeof(buffer)) {
        UARTprintf("Could not send SNTP request\n\r\n\r");
        close(sock);
        return 0;    // could not send SNTP request
    }

    //
    // Wait to receive the NTP time from the server
    //
    iAddrSize = sizeof(SlSockAddrIn_t);
    sLocalAddr.sin_family = SL_AF_INET;
    sLocalAddr.sin_port = 0;
    sLocalAddr.sin_addr.s_addr = 0;
    bind(sock, (SlSockAddr_t *) &sLocalAddr, iAddrSize);

    UARTprintf("receiving reply\n\r\n\r");

    rv = recvfrom(sock, buffer, sizeof(buffer), 0, (SlSockAddr_t *) &sLocalAddr,
            (SlSocklen_t*) &iAddrSize);
    if (rv <= 0) {
        UARTprintf("Did not receive\n\r");
        close(sock);
        return 0;
    }

    //
    // Confirm that the MODE is 4 --> server
    if ((buffer[0] & 0x7) != 4)    // expect only server response
            {
        UARTprintf("Expecting response from Server Only!\n\r");
        close(sock);
        return 0;    // MODE is not server, abort
    } else {
        //
        // Getting the data from the Transmit Timestamp (seconds) field
        // This is the time at which the reply departed the
        // server for the client
        //
        ntp = buffer[40];
        ntp <<= 8;
        ntp += buffer[41];
        ntp <<= 8;
        ntp += buffer[42];
        ntp <<= 8;
        ntp += buffer[43];

        ntp -= 2208988800UL;

        close(sock);
    }
    return (unsigned long) ntp;
}

unsigned long get_time();

int Cmd_time(int argc, char*argv[]) {
	int unix = unix_time();
	int t = get_time();

    UARTprintf(" time is %u and the ntp is %d and the diff is %d\n", t, unix, t-unix);

    return 0;
}

int Cmd_mode(int argc, char*argv[]) {
    int ap = 0;
    if (argc != 2) {
        UARTprintf("mode <1=ap 0=station>\n");
    }
    ap = atoi(argv[1]);
    if (ap && sl_mode != ROLE_AP) {
        //Switch to AP Mode
        sl_WlanSetMode(ROLE_AP);
        sl_Stop(SL_STOP_TIMEOUT);
        sl_mode = sl_Start(NULL, NULL, NULL);
    }
    if (!ap && sl_mode != ROLE_STA) {
        //Switch to STA Mode
        nwp_reset();
    }

    return 0;
}
#include "crypto.h"
static uint8_t aes_key[AES_BLOCKSIZE + 1] = "1234567891234567";

int Cmd_set_aes(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	unsigned long tok=0;
	long hndl, bytes;
	SlFsFileInfo_t info;
    int i;
    char* next = &argv[1][0];
    char *pend;

    for( i=0; i<AES_BLOCKSIZE/2;++i) {
    	aes_key[i] = strtol(next, &pend, 16);
        next = pend+1;
    }

	sl_FsGetInfo((unsigned char*)AES_KEY_LOC, tok, &info);

	if (sl_FsOpen((unsigned char*)AES_KEY_LOC,
	FS_MODE_OPEN_WRITE, &tok, &hndl)) {
		UARTprintf("error opening file, trying to create\n");

		if (sl_FsOpen((unsigned char*)AES_KEY_LOC,
				FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok,
				&hndl)) {
			UARTprintf("error opening for write\n");
			return -1;
		}
	}

	bytes = sl_FsWrite(hndl, info.FileLen, aes_key, AES_BLOCKSIZE);
	UARTprintf("wrote to the file %d bytes\n", bytes);

	sl_FsClose(hndl, 0, 0, 0);

	// Return success.
	return (0);
}

int Cmd_set_mac(int argc, char*argv[]) {
    uint8_t MAC_Address[6];
    int i;
    char* pend;
    char* next = &argv[1][0];

    for( i=0; i<6;++i) {
        MAC_Address[i] = strtol(next, &pend, 16);
        next = pend+1;
    }

    sl_NetCfgSet(SL_MAC_ADDRESS_SET,1,SL_MAC_ADDR_LEN,(_u8 *)MAC_Address);
    sl_Stop(0);
    sl_Start(NULL,NULL,NULL);

    return 0;
}

void load_aes() {
	long DeviceFileHandle = -1;
	int RetVal, Offset;

	// read in aes key
	RetVal = sl_FsOpen(AES_KEY_LOC, FS_MODE_OPEN_READ, NULL,
			&DeviceFileHandle);
	if (RetVal != 0) {
		UARTprintf("failed to open aes key file\n");
	}

	Offset = 0;
	RetVal = sl_FsRead(DeviceFileHandle, Offset, (unsigned char *) aes_key,
			AES_BLOCKSIZE);
	if (RetVal != AES_BLOCKSIZE) {
		UARTprintf("failed to read aes key file\n");
	}
	aes_key[AES_BLOCKSIZE] = 0;
	UARTprintf("read key %s\n", aes_key);

	RetVal = sl_FsClose(DeviceFileHandle, NULL, NULL, 0);
}

/* protobuf includes */
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "envelope.pb.h"
#include "periodic.pb.h"
#include "audio_data.pb.h"

static SHA1_CTX sha1ctx;

typedef struct {
	intptr_t fd;
	uint8_t * buf;
	uint32_t buf_pos;
	uint32_t buf_size;
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

static bool flush_out_buffer(ostream_buffered_desc_t * desc) {
	uint32_t buf_size = desc->buf_pos;
	bool ret = true;

	if (buf_size > 0) {
		desc->bytes_written += buf_size;

		//encrypt
		SHA1_Update(desc->ctx, desc->buf, buf_size);

		//send
		ret = send(desc->fd, desc->buf, buf_size, 0) == buf_size;
	}
	return ret;
}

static bool write_buffered_callback_sha(pb_ostream_t *stream, const uint8_t * inbuf,
        size_t count) {
	ostream_buffered_desc_t * desc = (ostream_buffered_desc_t *) stream->state;

	bool ret = true;

	desc->bytes_that_should_have_been_written += count;

	/* Will I exceed the buffer size? then send buffer */
	if ( (desc->buf_pos + count ) >= desc->buf_size) {

		//encrypt
		SHA1_Update(desc->ctx, desc->buf, desc->buf_pos);

		//send
		ret = send(desc->fd, desc->buf, desc->buf_pos, 0) == desc->buf_pos;

		desc->bytes_written += desc->buf_pos;

		desc->buf_pos = 0;

	}


	//copy to our buffer
	memcpy(desc->buf + desc->buf_pos,inbuf,count);
	desc->buf_pos += count;


    return ret;
}

static bool read_callback_sha(pb_istream_t *stream, uint8_t *buf, size_t count) {
    int fd = (intptr_t) stream->state;
    int result,i;

    result = recv(fd, buf, count, 0);

    SHA1_Update(&sha1ctx, buf, count);

    for (i = 0; i < count; ++i) {
        UARTprintf("%x", buf);
    }

    if (result == 0)
        stream->bytes_left = 0; /* EOF */

    return result == count;
}



//WARNING not re-entrant! Only 1 of these can be going at a time!
pb_ostream_t pb_ostream_from_sha_socket(ostream_buffered_desc_t * desc) {

    //pb_ostream_t stream = { write_callback_sha, (void*)desc, SIZE_MAX, 0 };
    pb_ostream_t stream = { write_buffered_callback_sha, (void*)desc, SIZE_MAX, 0 };



    return stream;
}

pb_istream_t pb_istream_from_sha_socket(int fd) {
    pb_istream_t stream = { &read_callback_sha, (void*) (intptr_t) fd, SIZE_MAX };
    return stream;
}

static int sock = -1;
static unsigned long ipaddr = 0;

#include "fault.h"

void UARTprintfFaults() {
#define minval( a,b ) a < b ? a : b
#define BUF_SZ 600
    size_t message_length;

    faultInfo * info = (faultInfo*)SHUTDOWN_MEM;

    if (info->magic == SHUTDOWN_MAGIC) {
        char buffer[BUF_SZ];
        if (sock > 0) {
            message_length = sizeof(faultInfo);
            snprintf(buffer, sizeof(buffer), "POST /in/morpheus/fault HTTP/1.1\r\n"
                    "Host: in.skeletor.com\r\n"
                    "Content-type: application/x-protobuf\r\n"
                    "Content-length: %d\r\n"
                    "\r\n", message_length);
            memcpy(buffer + strlen(buffer), info, sizeof(faultInfo));

            UARTprintf("sending faultdata\n\r\n\r");

            send(sock, buffer, strlen(buffer), 0);

            recv(sock, buffer, sizeof(buffer), 0);

            //UARTprintf("Reply is:\n\r\n\r");
            //buffer[127] = 0; //make sure it terminates..
            //UARTprintf("%s", buffer);

            info->magic = 0;
        }
    }
}

int stop_connection() {
    close(sock);
    sock = -1;
    return sock;
}
int start_connection() {
    sockaddr sAddr;
    timeval tv;
    int rv;
    int sock_begin = sock;

    if (sock < 0) {
        sock = socket(AF_INET, SOCK_STREAM, SL_SEC_SOCKET);
        tv.tv_sec = 2;             // Seconds
        tv.tv_usec = 0;           // Microseconds. 10000 microseconds resolution
        setsockopt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

        #define SL_SSL_CA_CERT_FILE_NAME "/cert/ca.der"
        // configure the socket as SSLV3.0
        // configure the socket as RSA with RC4 128 SHA
        // setup certificate
        unsigned char method = SL_SO_SEC_METHOD_TLSV1_2;
        unsigned int cipher = SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA;
        if( sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECMETHOD, &method, sizeof(method) ) < 0 ||
            sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &cipher, sizeof(cipher)) < 0 ||
            sl_SetSockOpt(sock, SL_SOL_SOCKET, \
                                   SL_SO_SECURE_FILES_CA_FILE_NAME, \
                                   SL_SSL_CA_CERT_FILE_NAME, \
                                   strlen(SL_SSL_CA_CERT_FILE_NAME))  < 0  )
        {
        UARTprintf( "error setting ssl options\r\n" );
        }
    }

    if (sock < 0) {
        UARTprintf("Socket create failed %d\n\r", sock);
        return -1;
    }
    UARTprintf("Socket created\n\r");

#if !LOCAL_TEST
    if (ipaddr == 0) {
        if (!(rv = gethostbyname(DATA_SERVER, strlen(DATA_SERVER), &ipaddr,
        SL_AF_INET))) {
            /*    UARTprintf(
             "Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
             DATA_SERVER, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
             SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
             */
        } else {
            UARTprintf("failed to resolve ntp addr rv %d\n");
            return -1;
        }
    }

    sAddr.sa_family = AF_INET;
    // the source port
    sAddr.sa_data[0] = 1;    //port 443, ssl
    sAddr.sa_data[1] = 0xbb; //port 443, ssl
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

    //connect it up
    //UARTprintf("Connecting \n\r\n\r");
    if (sock > 0 && sock_begin < 0 && (rv = connect(sock, &sAddr, sizeof(sAddr)))) {
        UARTprintf("connect returned %d\n\r\n\r", rv);
        if (rv != SL_ESECSNOVERIFY) {
            UARTprintf("Could not connect %d\n\r\n\r", rv);
            return stop_connection();    // could not send SNTP request
        }
    }
    return 0;
}

//takes the buffer and reads <return value> bytes, up to buffer size
typedef int(*audio_read_cb)(char * buffer, int buffer_size);

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

    snprintf(buffer, buffer_size, "POST /audio/%x%x%x%x%x%x HTTP/1.1\r\n"
            "Host: dev-in.hello.com\r\n"
            "Content-type: application/octet-stream\r\n"
            "Content-length: %d\r\n"
            "\r\n", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], message_length);
    send_length = strlen(buffer);

    UARTprintf("%s\n\r\n\r", buffer);

    //setup the connection
    if( start_connection() < 0 ) {
        UARTprintf("failed to start connection\n\r\n\r");
        return -1;
    }

    //UARTprintf("Sending request\n\r%s\n\r", buffer);
    rv = send(sock, buffer, send_length, 0);
    if ( rv <= 0) {
        UARTprintf("send error %d\n\r\n\r", rv);
        return stop_connection();
    }
    UARTprintf("sent %d\n\r\n\r", rv);

    while( message_length > 0 ) {
        int buffer_sent, buffer_read;
        buffer_sent = buffer_read = arcb( buffer, buffer_size );
        UARTprintf("read %d\n\r", buffer_read);

        while( buffer_read > 0 ) {
            send_length = minval(buffer_size, buffer_read);
            UARTprintf("attempting to send %d\n\r", send_length );

            rv = send(sock, buffer, send_length, 0);
            if (rv <= 0) {
                UARTprintf("send error %d\n\r", rv);
                return stop_connection();
            }
            UARTprintf("sent %d, %d left in buffer\n\r", rv, buffer_read);
            buffer_read -= rv;
        }
        message_length -= buffer_sent;
        UARTprintf("sent buffer, %d left\n\r\n\r", message_length);
    }

    memset(buffer, 0, buffer_size);

    //UARTprintf("Waiting for reply\n\r\n\r");
    rv = recv(sock, buffer, buffer_size, 0);
    if (rv <= 0) {
        UARTprintf("recv error %d\n\r\n\r", rv);
        return stop_connection();
    }
    UARTprintf("recv %d\n\r\n\r", rv);

    //UARTprintf("Reply is:\n\r\n\r");
    //buffer[127] = 0; //make sure it terminates..
    //UARTprintf("%s", buffer);


    return 0;
}

#if 1
#define SIG_SIZE 32
#include "SyncResponse.pb.h"


int decode_rx_data_pb_callback(const uint8_t * buffer, uint32_t buffer_size, void * decodedata,network_decode_callback_t decoder) {
	AES_CTX aesctx;
	unsigned char * buf_pos = (unsigned char*)buffer;
	unsigned char sig[SIG_SIZE] = {0};
	unsigned char sig_test[SIG_SIZE] = {0};
	int i;
	int status;
	pb_istream_t stream;


	if (!decoder) {
		return -1;
	}


	//memset( aesctx.iv, 0, sizeof( aesctx.iv ) );

	UARTprintf("iv ");
	for (i = 0; i < AES_IV_SIZE; ++i) {
		aesctx.iv[i] = *buf_pos++;
		UARTprintf("%02x", aesctx.iv[i]);
		if (buf_pos > (buffer + buffer_size)) {
			return -1;
		}
	}
	UARTprintf("\n");
	UARTprintf("sig");
	for (i = 0; i < SIG_SIZE; ++i) {
		sig[i] = *buf_pos++;
		UARTprintf("%02x", sig[i]);
		if (buf_pos > (buffer + buffer_size)) {
			return -1;
		}
	}
	UARTprintf("\n");
	buffer_size -= (SIG_SIZE + AES_IV_SIZE);

	AES_set_key(&aesctx, aes_key, aesctx.iv, AES_MODE_128); //TODO: real key
	AES_convert_key(&aesctx);
	AES_cbc_decrypt(&aesctx, sig, sig, SIG_SIZE);

	SHA1_Init(&sha1ctx);
	SHA1_Update(&sha1ctx, buf_pos, buffer_size);
	//now verify sig
	SHA1_Final(sig_test, &sha1ctx);
	if (memcmp(sig, sig_test, SHA1_SIZE) != 0) {
		UARTprintf("signatures do not match\n");
		for (i = 0; i < SHA1_SIZE; ++i) {
			UARTprintf("%02x", sig[i]);
		}
		UARTprintf("\nvs\n");
		for (i = 0; i < SHA1_SIZE; ++i) {
			UARTprintf("%02x", sig_test[i]);
		}
		UARTprintf("\n");

		return -1; //todo uncomment
	}

	/* Create a stream that will read from our buffer. */
	stream = pb_istream_from_buffer(buf_pos, buffer_size);
	/* Now we are ready to decode the message! */

	UARTprintf("data ");
	status = decoder(&stream,decodedata);
	UARTprintf("\n");

	/* Then just check for any errors.. */
	if (!status) {
		UARTprintf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		return -1;
	}

	return 0;
}




static uint32_t default_decode_callback(pb_istream_t * stream, void * data) {
	network_decode_data_t * decoderinfo = (network_decode_data_t *) data;
	return pb_decode(stream,decoderinfo->fields,decoderinfo->decodedata);
}

int decode_rx_data_pb(const uint8_t * buffer, uint32_t buffer_size, const pb_field_t fields[],void * structdata) {
	network_decode_data_t decode_data;
	int ret;

	decode_data.fields = fields;
	decode_data.decodedata = structdata;

	ret = decode_rx_data_pb_callback(buffer,buffer_size,&decode_data,default_decode_callback);

	return ret;
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

//buffer needs to be at least 128 bytes...

int send_data_pb_callback(const char* host, const char* path,char * recv_buf, uint32_t recv_buf_size,const void * encodedata,network_encode_callback_t encoder) {

    int send_length = 0;
    int rv = 0;
    uint8_t sig[32]={0};
    size_t message_length;
    uint32_t null_stream_bytes = 0;
    bool status;


    if (!recv_buf) {
    	UARTprintf("send_data_pb_callback needs a buffer\r\n");
    	return -1;
    }

    if (encoder) {
        pb_ostream_t size_stream = {0};
        status = encoder(&size_stream, encodedata);
        if(!status)
        {
            UARTprintf("Encode protobuf failed, %s\n", PB_GET_ERROR(&size_stream));
            return -1;
        }


        null_stream_bytes = size_stream.bytes_written;
        message_length = size_stream.bytes_written + sizeof(sig) + AES_IV_SIZE;
        UARTprintf("message len %d sig len %d\n\r\n\r", size_stream.bytes_written, sizeof(sig));
    }

    snprintf(recv_buf, recv_buf_size, "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-type: application/x-protobuf\r\n"
            "Content-length: %d\r\n"
            "\r\n", path, host, message_length);

    send_length = strlen(recv_buf);

    //setup the connection
    if( start_connection() < 0 ) {
        UARTprintf("failed to start connection\n\r\n\r");
        return -1;
    }

    //UARTprintf("Sending request\n\r%s\n\r", recv_buf);
    rv = send(sock, recv_buf, send_length, 0);
    if (rv <= 0) {
        UARTprintf("send error %d\n\r\n\r", rv);
        return stop_connection();
    }

    UARTprintf("HTTP header sent %d\n\r%s\n\r", rv, recv_buf);


    if (encoder) {
        ostream_buffered_desc_t desc;

        SHA1_CTX ctx;
        AES_CTX aesctx;
        pb_ostream_t stream = {0};
        int i;

        memset(&desc,0,sizeof(desc));
        memset(&ctx,0,sizeof(ctx));
        memset(&aesctx,0,sizeof(aesctx));

        desc.buf = (uint8_t *)recv_buf;
        desc.buf_size = recv_buf_size;
        desc.ctx = &ctx;
        desc.fd = (intptr_t) sock;


        SHA1_Init(&ctx);

        /* Create a stream that will write to our buffer. */
        stream = pb_ostream_from_sha_socket(&desc);

        /* Now we are ready to encode the message! Let's go encode. */
        UARTprintf("data ");
        status = encoder(&stream,encodedata);
        flush_out_buffer(&desc);
        UARTprintf("\n");

        /* sanity checks  */
        if (desc.bytes_written != desc.bytes_that_should_have_been_written) {
        	UARTprintf("ERROR only %d of %d bytes written\r\n",desc.bytes_written,desc.bytes_that_should_have_been_written);
        }

        if (desc.bytes_written != null_stream_bytes) {
        	UARTprintf("ERROR %d bytes estimated, %d bytes were sent\r\n",null_stream_bytes,desc.bytes_written);
        }

        /* Then just check for any errors.. */
        if (!status) {
            UARTprintf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return -1;
        }

        //now sign it
        SHA1_Final(sig, &ctx);

        /*  fill in rest of signature with random numbers (eh?) */
        for (i = SHA1_SIZE; i < sizeof(sig); ++i) {
            sig[i] = (uint8_t)rand();
        }

        UARTprintf("SHA ");
        for (i = 0; i < sizeof(sig); ++i) {
            UARTprintf("%x", sig[i]);
        }
        UARTprintf("\n");

        //memset( aesctx.iv, 0, sizeof( aesctx.iv ) );

        /*  create AES initialization vector */
        UARTprintf("iv ");
        for (i = 0; i < sizeof(aesctx.iv); ++i) {
            aesctx.iv[i] = (uint8_t)rand();
            UARTprintf("%x", aesctx.iv[i]);
        }
        UARTprintf("\n");

        /*  send AES initialization vector */
        rv = send(sock, aesctx.iv, AES_IV_SIZE, 0);


        if (rv != AES_IV_SIZE) {
            UARTprintf("Sending IV failed: %d\n", rv);
            return -1;
        }

        AES_set_key(&aesctx, aes_key, aesctx.iv, AES_MODE_128); //todo real key
        AES_cbc_encrypt(&aesctx, sig, sig, sizeof(sig));

        /* send signature */
        rv = send(sock, sig, sizeof(sig), 0);

        if (rv != sizeof(sig)) {
            UARTprintf("Sending SHA failed: %d\n", rv);
            return -1;
        }

        UARTprintf("sig ");
        for (i = 0; i < sizeof(sig); ++i) {
            UARTprintf("%x", sig[i]);
        }
        UARTprintf("\n");
    }


    memset(recv_buf, 0, recv_buf_size);

    //UARTprintf("Waiting for reply\n\r\n\r");
    rv = recv(sock, recv_buf, recv_buf_size, 0);
    if (rv <= 0) {
        UARTprintf("recv error %d\n\r\n\r", rv);
        return stop_connection();
    }
    UARTprintf("recv %d\n\r\n\r", rv);

	UARTprintf("Send complete\n");

    //todo check for http response code 2xx

    //UARTprintfFaults();
    //close( sock ); //never close our precious socket

    return 0;
}




bool encode_pill_id(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	char* str = *arg;
	if(!str)
	{
		UARTprintf("encode_pill_id: No data to encode\n");
		return 0;
	}

	if (!pb_encode_tag_for_field(stream, field)){
		UARTprintf("encode_pill_id: Fail to encode tag\n");
		return 0;
	}

	return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

int http_response_ok(const char* response_buffer)
{
	char* first_line = strstr(response_buffer, "\r\n") + 2;
	uint16_t first_line_len = first_line - response_buffer;
	if(!first_line_len > 0)
	{
		UARTprintf("Cannot get response first line.\n");
		return -1;
	}
	first_line = pvPortMalloc(first_line_len + 1);
	if(!first_line)
	{
		UARTprintf("No memory\n");
		return -2;
	}

	memset(first_line, 0, first_line_len + 1);
	memcpy(first_line, response_buffer, first_line_len);
	UARTprintf("Status line: %s\n", first_line);

	int resp_ok = match("2..", first_line);
	int ret = 0;
	if (resp_ok) {
		ret = 1;
	}
	vPortFree(first_line);
	return ret;
}

#include "ble_cmd.h"
static bool _encode_encrypted_pilldata(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const array_data* array_holder = (array_data*)(*arg);
    if(!array_holder)
    {
    	UARTprintf("_encode_encrypted_pilldata: No data to encode\n");
        return false;
    }

    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag))
    {
    	UARTprintf("_encode_encrypted_pilldata: Fail to encode tag\n");
        return false;
    }

    return pb_encode_string(stream, array_holder->buffer, array_holder->length);
}

bool encode_pill_list(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	periodic_data_pill_data_container* ptr_pill_list = *(periodic_data_pill_data_container**) arg;
    if(!ptr_pill_list)
    {
        UARTprintf("No pill list to encode\n");
        return 0;
    }


	int i;
	
    for (i = 0; i < MAX_PILLS; ++i) {
        periodic_data_pill_data_container data = ptr_pill_list[i];
		if( data.magic != PILL_MAGIC ) {
            continue;
		}

		data.pill_data.deviceId.funcs.encode = encode_pill_id;
		data.pill_data.deviceId.arg = data.id; // attach the id to protobuf structure.
        if(data.pill_data.motionDataEncrypted.arg &&
            NULL == data.pill_data.motionDataEncrypted.funcs.encode)
        {
            // Set the default encode function for encrypted motion data.
            data.pill_data.motionDataEncrypted.funcs.encode = _encode_encrypted_pilldata;
        }
	  
        /*
		pb_ostream_t sizestream = { 0 };
		if(!pb_encode(&sizestream, periodic_data_pill_data_fields, &data->pill_data))
        {
            UARTprintf("Fail to encode pill %s\r\n", data->id);
            continue;
        }
        */

        if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)){
        	UARTprintf("Fail to encode tag for pill %s\r\n", data.id);
        	continue;
		}

        pb_ostream_t sizestream = { 0 };
        if(!pb_encode(&sizestream, periodic_data_pill_data_fields, &data.pill_data)){
        	UARTprintf("Failed to retreive length\n");
        	continue;
        }

        if (!pb_encode_varint(stream, sizestream.bytes_written)){
        	UARTprintf("Failed to write length\n");
			continue;
        }

		if (!pb_encode(stream, periodic_data_pill_data_fields, &data.pill_data)){
			UARTprintf("Fail to encode pill %s\r\n", data.id);
            continue;
		}else{
			UARTprintf("Pill %s data uploaded\n", data.id);
		}

    }

    return 1;
    
}


bool get_mac(unsigned char mac[6]) {
	int32_t ret;
	unsigned char mac_len = 6;

	ret = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);

    if(ret != 0 && ret != SL_ESMALLBUF)
    {
    	UARTprintf("encode_mac_as_device_id_string: Fail to get MAC addr, err %d\n", ret);
        return false;  // If get mac failed, don't encode that field
    }



    return ret;
}

bool encode_mac(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
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
    int32_t ret = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
    if(ret != 0 && ret != SL_ESMALLBUF)
    {
    	UARTprintf("encode_mac: Fail to get MAC addr, err %d\n", ret);
        return false;  // If get mac failed, don't encode that field
    }
#endif

    return pb_encode_tag(stream, PB_WT_STRING, field->tag) && pb_encode_string(stream, (uint8_t*) mac, mac_len);
}

bool encode_mac_as_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    uint8_t mac[6] = {0};
    uint8_t mac_len = 6;
#if FAKE_MAC
    mac[0] = 0xab;
    mac[1] = 0xcd;
    mac[2] = 0xab;
    mac[3] = 0xcd;
    mac[4] = 0xab;
    mac[5] = 0xcd;
#else
    int32_t ret = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
    if(ret != 0 && ret != SL_ESMALLBUF)
    {
    	UARTprintf("encode_mac_as_device_id_string: Fail to get MAC addr, err %d\n", ret);
        return false;  // If get mac failed, don't encode that field
    }
#endif
    char hex_device_id[13] = {0};
    uint8_t i = 0;
    for(i = 0; i < sizeof(mac); i++){
    	snprintf(&hex_device_id[i * 2], 3, "%02X", mac[i]);
    }

    return pb_encode_tag_for_field(stream, field) && pb_encode_string(stream, (uint8_t*)hex_device_id, strlen(hex_device_id));
}

bool encode_name(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    return pb_encode_tag(stream, PB_WT_STRING, field->tag)
            && pb_encode_string(stream, (uint8_t*) MORPH_NAME,
                    strlen(MORPH_NAME));
}


bool encode_serialized_pill_list(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	array_data* array_holder = (array_data*)arg;
    if(!array_holder)
    {
        UARTprintf("No pill data to encode\n");
        return 0;
    }

    // The serilized pill list already has tags encoded, don't need to write tag anymore
    // just write the bytes straight into the stream.
    UARTprintf("raw len: %d, tag: %d\n", array_holder->length, field->tag);
    return pb_write(stream, array_holder->buffer, array_holder->length);
}

void encode_pill_list_to_buffer(const periodic_data_pill_data_container* ptr_pill_list,
    uint8_t* buffer, size_t buffer_len, size_t* out_len)
{
    pb_ostream_t stream = {0};
    *out_len = 0;

    if(!buffer)
    {
        stream = pb_ostream_from_buffer(buffer, buffer_len);
    }
    memset(buffer, 0, buffer_len);

	int i;

	for (i = 0; i < MAX_PILLS; ++i) {
		periodic_data_pill_data_container data = ptr_pill_list[i];
		if( data.magic != PILL_MAGIC ) {
			continue;
		}

		data.pill_data.deviceId.funcs.encode = encode_pill_id;
		data.pill_data.deviceId.arg = data.id; // attach the id to protobuf structure.
		if(data.pill_data.motionDataEncrypted.arg &&
			NULL == data.pill_data.motionDataEncrypted.funcs.encode)
		{
			// Set the default encode function for encrypted motion data.
			data.pill_data.motionDataEncrypted.funcs.encode = _encode_encrypted_pilldata;
		}


		if(!buffer)
		{

			pb_ostream_t sizestream = {0};
			size_t message_len = 0;
			if (!pb_encode(&sizestream, periodic_data_pill_data_fields, &data.pill_data)){
				UARTprintf("Fail to encode pill %s\r\n", data.id);
				continue;
			}

			message_len = sizestream.bytes_written;  // The payload len

			if (!pb_encode_varint(&sizestream, message_len)){
				UARTprintf("Failed to get length\n");
				continue;
			}

			message_len = sizestream.bytes_written;  // len + payload len

			size_t len = message_len + 10;  // This is a must or we can't know the actual length including the tag
			uint8_t* size_buffer = pvPortMalloc(len);
			if(!size_buffer)
			{
				continue;
			}

			memset(size_buffer, 0, len);
			sizestream = pb_ostream_from_buffer(size_buffer, len);

			//UARTprintf("start: %d\n", sizestream.bytes_written);
			if (!pb_encode_tag(&sizestream, PB_WT_STRING, periodic_data_pills_tag))
			{
				UARTprintf("encode_pill_list_to_buffer: Fail to encode tag for pill %s, error %s\n", data.id, PB_GET_ERROR(&sizestream));
			}else{
				UARTprintf("tag len: %d, tag_val: %d\n", sizestream.bytes_written, size_buffer[0]);
				message_len += sizestream.bytes_written;

				UARTprintf("tag+len+message: %d\n", message_len);
				*out_len += message_len;
			}
			vPortFree(size_buffer);


		}else{

			size_t message_len, stream_len;
			stream_len = stream.bytes_written;
			if (!pb_encode_tag(&stream, PB_WT_STRING, periodic_data_pills_tag))
			{
				UARTprintf("encode_pill_list_to_buffer: Fail to encode tag for pill %s, error %s\n", data.id, PB_GET_ERROR(&stream));
				continue;
			}
			message_len = stream.bytes_written - stream_len;
			stream_len = stream.bytes_written;

			/*
			if (!pb_encode_delimited(&stream, periodic_data_pill_data_fields, &data.pill_data)){
				UARTprintf("encode_pill_list_to_buffer: Fail to encode pill %s, error: %s\n", data.id, PB_GET_ERROR(&stream));
				continue;
			}
			*/

			pb_ostream_t sizestream = { 0 };
			if(!pb_encode(&sizestream, periodic_data_pill_data_fields, &data.pill_data)){
				//UARTprintf("Failed to retreive length\n");
				continue;
			}

			UARTprintf("message: %d\n", sizestream.bytes_written);



			if (!pb_encode_varint(&stream, sizestream.bytes_written)){
				//UARTprintf("Failed to write length\n");
				continue;
			}

			message_len += stream.bytes_written - stream_len;
			stream_len = stream.bytes_written;

			if (!pb_encode(&stream, periodic_data_pill_data_fields, &data.pill_data)){
				UARTprintf("Fail to encode pill %s\n", data.id);
				continue;
			}

			message_len += stream.bytes_written - stream_len;
			stream_len = stream.bytes_written;

			*out_len += message_len;
		}
    }

    UARTprintf("total len: %d\n", *out_len);
}

static void _on_alarm_received(const SyncResponse_Alarm* received_alarm)
{
    if (xSemaphoreTake(alarm_smphr, portMAX_DELAY)) {
        if (received_alarm->has_start_time && received_alarm->start_time > 0) {
            if (get_time() > received_alarm->start_time) {
                // This approach is error prond: We got information from two different sources
                // and expect them consistent. The time in our server might be different with NTP.
                // I am going to redesign this, instead of returning start/end timestamp, the backend
                // will retrun the offset seconds from now to the next ring and the ring duration.
                // So we don't need to care the actual time of 'Now'.
                memcpy(&alarm, received_alarm, sizeof(alarm));
            }
            UARTprintf("Got alarm %d to %d in %d minutes\n",
            		received_alarm->start_time, received_alarm->end_time,
                    (received_alarm->start_time - get_time()) / 60);
        }else{
            UARTprintf("No alarm for now.\n");
        }

        xSemaphoreGive(alarm_smphr);
    }
}

static void _on_factory_reset_received()
{
    // hehe I am going to disconnect WLAN here, don't kill me Chris
    wifi_reset();
    nwp_reset();

    // Notify the topboard factory reset, wipe out all whitelist info
    MorpheusCommand morpheusCommand = {0};
    morpheusCommand.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET;
    ble_send_protobuf(&morpheusCommand);  // Send the protobuf to topboard
}

static void _on_response_protobuf(const SyncResponse* response_protobuf)
{
    if (response_protobuf->has_alarm) {
        _on_alarm_received(&response_protobuf->alarm);
    }

    if(response_protobuf->has_reset_device && response_protobuf->reset_device){
        UARTprintf("Server factory reset.\n");
        
        _on_factory_reset_received();
    }
}


int send_periodic_data(periodic_data* data) {
    char buffer[256] = {0};

    int ret;

    UARTprintf("1111111111111111111111111\n");

    /*
    pb_ostream_t size_stream = {0};
    if(pb_encode(&size_stream, periodic_data_fields, data))
    {
    	uint8_t* proto_buffer = pvPortMalloc(size_stream.bytes_written);
    	if(!buffer)
    	{
    		return -1;
    	}

    	memset(proto_buffer, 0, size_stream.bytes_written);
    	pb_ostream_t buffer_stream = pb_ostream_from_buffer(proto_buffer, size_stream.bytes_written);
    	pb_encode(&buffer_stream, periodic_data_fields, data);

    	array_data holder = {0};
    	holder.buffer = proto_buffer;
    	holder.length = buffer_stream.bytes_written;

    	ret = NetworkTask_SynchronousSendRawProtobuf(DATA_RECEIVE_ENDPOINT, &holder, buffer, sizeof(buffer), 0);
    	vPortFree(proto_buffer);

    	// Parse the response
		UARTprintf("Reply is:\n\r%s\n\r", buffer);

		const char* header_content_len = "Content-Length: ";
		char * content = strstr(buffer, "\r\n\r\n") + 4;
		char * len_str = strstr(buffer, header_content_len) + strlen(header_content_len);
		if (http_response_ok(buffer) != 1) {
			sl_status &= ~UPLOADING;
			UARTprintf("Invalid response, endpoint return failure.\n");
			return -1;
		}

		if (len_str == NULL) {
			sl_status &= ~UPLOADING;
			UARTprintf("Failed to find Content-Length header\n");
			return -1;
		}
		int len = atoi(len_str);

		SyncResponse response_protobuf;
		memset(&response_protobuf, 0, sizeof(response_protobuf));

		if(decode_rx_data_pb((unsigned char*) content, len, SyncResponse_fields, &response_protobuf) == 0)
		{
			UARTprintf("Decoding success: %d %d %d %d %d %d\n",
			response_protobuf.has_acc_sampling_interval,
			response_protobuf.has_acc_scan_cyle,
			response_protobuf.has_alarm,
			response_protobuf.has_device_sampling_interval,
			response_protobuf.has_flash_action,
			response_protobuf.has_reset_device);

			_on_response_protobuf(&response_protobuf);
			sl_status |= UPLOADING;

			return 0;
		}

    }else{
    	UARTprintf("Get size failed\n");
    }
	*/

    /* */
    //set this to zero--it won't retry, since retrying is handled by an outside loop
    ret = NetworkTask_SynchronousSendProtobuf(DATA_RECEIVE_ENDPOINT, buffer, sizeof(buffer), periodic_data_fields, &data, 0);
    UARTprintf("2222222222222222222222222\n");
    if(ret != 0)
    {
        // network error
    	sl_status &= ~UPLOADING;
        UARTprintf("Send data failed, network error %d\n", ret);
        return ret;
    }

    // Parse the response
    UARTprintf("Reply is:\n\r%s\n\r", buffer);
    
    const char* header_content_len = "Content-Length: ";
    char * content = strstr(buffer, "\r\n\r\n") + 4;
    char * len_str = strstr(buffer, header_content_len) + strlen(header_content_len);
    if (http_response_ok(buffer) != 1) {
    	sl_status &= ~UPLOADING;
        UARTprintf("Invalid response, endpoint return failure.\n");
        return -1;
    }
    
    if (len_str == NULL) {
    	sl_status &= ~UPLOADING;
        UARTprintf("Failed to find Content-Length header\n");
        return -1;
    }
    int len = atoi(len_str);
    
    SyncResponse response_protobuf;
    memset(&response_protobuf, 0, sizeof(response_protobuf));

    if(decode_rx_data_pb((unsigned char*) content, len, SyncResponse_fields, &response_protobuf) == 0)
    {
        UARTprintf("Decoding success: %d %d %d %d %d %d\n",
        response_protobuf.has_acc_sampling_interval,
        response_protobuf.has_acc_scan_cyle,
        response_protobuf.has_alarm,
        response_protobuf.has_device_sampling_interval,
        response_protobuf.has_flash_action,
        response_protobuf.has_reset_device);

		_on_response_protobuf(&response_protobuf);
        sl_status |= UPLOADING;

        return 0;
    }
    /* */

    return -1;
}



int Cmd_data_upload(int arg, char* argv[])
{
	data_t data = {0};
	//load_aes();


	data.time = 1;
	data.light = 2;
	data.light_variability = 3;
	data.light_tonality = 4;
	data.temp = 5;
	data.humid = 6;
	data.dust = 7;
	data.pill_list = pill_list;
	UARTprintf("Debugging....\n");
	send_periodic_data(&data);

	return 0;
}


int Cmd_sl(int argc, char*argv[]) {

    unsigned char policyVal;

    //make sure we're in station mode
    if (sl_mode != ROLE_STA) {
        //Switch to STA Mode
        sl_WlanSetMode(ROLE_STA);
        sl_Stop(SL_STOP_TIMEOUT);
        sl_mode = sl_Start(NULL, NULL, NULL);
    }

    //sl_WlanProfileDel(WLAN_DEL_ALL_PROFILES);

    //set AUTO policy
    sl_WlanPolicySet( SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 0),
            &policyVal, 1 /*PolicyValLen*/);

    /* Start SmartConfig
     * This example uses the unsecured SmartConfig method
     */
    sl_WlanSmartConfigStart(0,                          //groupIdBitmask
            SMART_CONFIG_CIPHER_NONE,    //cipher
            0,                           //publicKeyLen
            0,                           //group1KeyLen
            0,                           //group2KeyLen
            NULL,                          //publicKey
            NULL,                          //group1Key
            NULL);                         //group2Key

    return 0;
}



#include "fatfs_cmd.h"

int audio_read_fn (char * buffer, int buffer_size) {
    memset(buffer, 0xabcd, buffer_size);

    return buffer_size;
}

int Cmd_audio_test(int argc, char *argv[]) {
    short audio[1024];
    send_audio_wifi( (char*)audio, sizeof(audio), audio_read_fn );
    return (0);
}

//radio test functions
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
		UARTprintf("startrx <channel>\n");
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

	UARTprintf( "valid_packets %d\n", valid_packets );
	UARTprintf( "fcs_packets %d\n", fcs_packets );
	UARTprintf( "plcp_packets %d\n", plcp_packets );
	UARTprintf( "avg_rssi_mgmt %d\n", avg_rssi_mgmt );
	UARTprintf( "acg_rssi_other %d\n", avg_rssi_other );

	UARTprintf("rssi histogram\n");
	for (i = 0; i < SIZE_OF_RSSI_HISTOGRAM; ++i) {
		UARTprintf("%d\n", rssi_histogram[i]);
	}
	UARTprintf("rate histogram\n");
	for (i = 0; i < NUM_OF_RATE_INDEXES; ++i) {
		UARTprintf("%d\n", rate_histogram[i]);
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
		UARTprintf("startx <mode, RADIO_TX_PACKETIZED=2, RADIO_TX_CW=3, RADIO_TX_CONTINUOUS=1> "
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
		UARTprintf("stoptx <mode, RADIO_TX_PACKETIZED=2, RADIO_TX_CW=3, RADIO_TX_CONTINUOUS=1>\n");
	}
	mode = (RadioTxMode_e)atoi(argv[1]);
	return RadioStopTX(mode);
}

int get_wifi_scan_result(Sl_WlanNetworkEntry_t* entries, uint16_t entry_len, uint32_t scan_duration_ms)
{
    if(scan_duration_ms < 1000)
    {
        return 0;
    }

    unsigned long IntervalVal = 20;

    unsigned char policyOpt = SL_CONNECTION_POLICY(0, 0, 0, 0, 0);
    int r;

    r = sl_WlanPolicySet(SL_POLICY_CONNECTION , policyOpt, NULL, 0);

    // Make sure scan is enabled
    policyOpt = SL_SCAN_POLICY(1);

    // set scan policy - this starts the scan
    r = sl_WlanPolicySet(SL_POLICY_SCAN , policyOpt, (unsigned char *)(IntervalVal), sizeof(IntervalVal));


    // delay specific milli seconds to verify scan is started
    vTaskDelay(scan_duration_ms);

    // r indicates the valid number of entries
    // The scan results are occupied in netEntries[]
    r = sl_WlanGetNetworkList(0, entry_len, entries);

    // Restore connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1),
            NULL, 0);

    return r;

}


int connect_wifi(const char* ssid, const char* password, int sec_type)
{
	SlSecParams_t secParam = {0};

	if(sec_type == 3 ) {
		sec_type = 2;
	}
	secParam.Key = (signed char*)password;
	secParam.KeyLen = password == NULL ? 0 : strlen(password);
	secParam.Type = sec_type;

	int16_t index = 0;
	int16_t ret = 0;
	uint8_t retry = 5;
	while((index = sl_WlanProfileAdd((_i8*) ssid, strlen(ssid), NULL,
			&secParam, NULL, 0, 0)) < 0 && retry--){
		ret = sl_WlanProfileDel(0xFF);
		if (ret != 0) {
			UARTprintf("profile del fail\n");
		}
	}

	if (index < 0) {
		UARTprintf("profile add fail\n");
		return 0;
	}
	ret = sl_WlanConnect((_i8*) ssid, strlen(ssid), NULL, sec_type == SL_SEC_TYPE_OPEN ? NULL : &secParam, 0);
	if(ret == 0 || ret == -71)
	{
		UARTprintf("WLAN connect attempt issued\n");

		return 1;
	}

	return 0;
}

int connect_scanned_endpoints(const char* ssid, const char* password, 
    const Sl_WlanNetworkEntry_t* wifi_endpoints, int scanned_wifi_count, SlSecParams_t* connectedEPSecParamsPtr)
{
	int16_t ret;
	if(!connectedEPSecParamsPtr)
	{
		return 0;
	}

	if(!wifi_endpoints)
	{
		return connect_wifi(ssid, password, connectedEPSecParamsPtr->Type);
	}

    int i = 0;

    for(i = 0; i < scanned_wifi_count; i++)
    {
        Sl_WlanNetworkEntry_t wifi_endpoint = wifi_endpoints[i];
        if(strcmp((const char*)wifi_endpoint.ssid, ssid) == 0)
        {
			ret = connect_wifi(ssid, password, wifi_endpoint.sec_type);
			if(ret)
			{
				return 1;
			}

        }
    }

    return 0;
}


//end radio test functions
