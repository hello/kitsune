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

#define ROLE_INVALID (-5)

int sl_mode = ROLE_INVALID;

unsigned int sl_status = 0;

#include "rom_map.h"
#include "prcm.h"
#include "utils.h"

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
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {

    switch (pNetAppEvent->Event) {
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
    case SL_NETAPP_IPV6_IPACQUIRED_EVENT:
        UARTprintf("SL_NETAPP_IPV4_ACQUIRED\n\r");
        sl_status |= HAS_IP;
        break;

    case SL_NETAPP_IP_LEASED_EVENT:
        sl_status |= IP_LEASED;
        break;
    default:
        break;
    }
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

    sl_WlanConnect((_i8*)argv[1], strlen(argv[1]), "123456", &secParams, 0);
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

int Cmd_time(int argc, char*argv[]) {

    UARTprintf(" time is %u \n", unix_time());

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

/* protobuf includes */
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "envelope.pb.h"
#include "periodic.pb.h"
#include "audio_data.pb.h"

static bool write_callback(pb_ostream_t *stream, const uint8_t *buf,
        size_t count) {
    int fd = (intptr_t) stream->state;
    return send(fd, buf, count, 0) == count;
}

static bool read_callback(pb_istream_t *stream, uint8_t *buf, size_t count) {
    int fd = (intptr_t) stream->state;
    int result;

    result = recv(fd, buf, count, 0);

    if (result == 0)
        stream->bytes_left = 0; /* EOF */

    return result == count;
}

pb_ostream_t pb_ostream_from_socket(int fd) {
    pb_ostream_t stream =
            { &write_callback, (void*) (intptr_t) fd, SIZE_MAX, 0 };
    return stream;
}

pb_istream_t pb_istream_from_socket(int fd) {
    pb_istream_t stream = { &read_callback, (void*) (intptr_t) fd, SIZE_MAX };
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

            UARTprintf("Reply is:\n\r\n\r");
            buffer[127] = 0; //make sure it terminates..
            UARTprintf("%s", buffer);

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

		#define SL_SSL_CA_CERT_FILE_NAME "/cert/hello.der"
        // configure the socket as SSLV3.0
        // configure the socket as RSA with RC4 128 SHA
        // setup certificate
        unsigned char method = SL_SO_SEC_METHOD_SSLV3;
        unsigned int cipher = SL_SEC_MASK_SSL_RSA_WITH_RC4_128_SHA;
        if( sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECMETHOD, &method, sizeof(method) ) < 0 ||
            sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &cipher, sizeof(cipher)) < 0 /*||
            sl_SetSockOpt(sock, SL_SOL_SOCKET, \
                                   SL_SO_SECURE_FILES_CA_FILE_NAME, \
                                   SL_SSL_CA_CERT_FILE_NAME, \
                                   strlen(SL_SSL_CA_CERT_FILE_NAME))  < 0 */ )
        {
        UARTprintf( "error setting ssl options\r\n" );
        }
    }

	if (sock < 0) {
		UARTprintf("Socket create failed %d\n\r", sock);
		return -1;
	}
	UARTprintf("Socket created\n\r");

#define DATA_SERVER "dev-in.hello.is"
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
int send_audio_wifi(char * buffer, int buffer_size, audio_read_cb arcb, int time) {
    int send_length;
    int rv = 0;
    int message_length;

    message_length = 110000;

    snprintf(buffer, buffer_size, "POST /in/morpheus/audio HTTP/1.1\r\n"
            "Host: in.skeletor.com\r\n"
            "Content-type: application/x-pcm16\r\n"
            "Content-length: %d\r\n"
            "\r\n", message_length);
    send_length = strlen(buffer);

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

	UARTprintf("Reply is:\n\r\n\r");
    buffer[127] = 0; //make sure it terminates..
    UARTprintf("%s", buffer);


    return 0;
}

//buffer needs to be at least 128 bytes...
int send_data_pb(char * buffer, int buffer_size, const pb_field_t fields[], const void *src_struct) {
    int send_length;
    int rv = 0;

    size_t message_length;
    bool status;

    {
        pb_ostream_t stream = {0};
        status = pb_encode(&stream, fields, src_struct);
        message_length = stream.bytes_written;
    }

    snprintf(buffer, buffer_size, "POST /in/morpheus/pb HTTP/1.1\r\n"
            "Host: in.skeletor.com\r\n"
            "Content-type: application/x-protobuf\r\n"
            "Content-length: %d\r\n"
            "\r\n", message_length);
    send_length = strlen(buffer);

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

    {
        /* Create a stream that will write to our buffer. */
        pb_ostream_t stream = pb_ostream_from_socket(sock);
        /* Now we are ready to encode the message! */
        status = pb_encode(&stream, fields, src_struct);
        message_length = stream.bytes_written;

        /* Then just check for any errors.. */
        if (!status) {
            UARTprintf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return -1;
        }

    }
    memset(buffer, 0, buffer_size);

    //UARTprintf("Waiting for reply\n\r\n\r");
    rv = recv(sock, buffer, buffer_size, 0);
	if (rv <= 0) {
		UARTprintf("recv error %d\n\r\n\r", rv);
        return stop_connection();
	}
	UARTprintf("recv %d\n\r\n\r", rv);

	UARTprintf("Reply is:\n\r\n\r");
    buffer[127] = 0; //make sure it terminates..
    UARTprintf("%s", buffer);

    //todo check for http response code 2xx

    //UARTprintfFaults();
    //close( sock ); //never close our precious socket

    return 0;
}


bool encode_audio_chunk(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    unsigned char tagtype = (6 << 3) | 0x2; // field_number << 3 | 2 (length deliminated)
    short audio[1024] = { 0xabcd };
    int audio_len = sizeof(audio);

    return pb_write(stream, &tagtype, 1) && pb_encode_string(stream, (uint8_t*) audio, audio_len);
}
bool encode_features(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    unsigned char tagtype = (1 << 3) | 0x2; // field_number << 3 | 2 (length deliminated)
    unsigned char features[16] = { 0xabcd };
    int len = sizeof(features);

    return pb_write(stream, &tagtype, 1) && pb_encode_string(stream, (uint8_t*) features, len);
}
int send_audio_chunk( int16_t * audio_chunk, int samples ) {

    char buffer[256];
    audio_data msg;

    //build the message
    msg.audio.funcs.encode = encode_audio_chunk;
    msg.audio_version = 1;
    msg.feature_version = 1;
    msg.features.funcs.encode = encode_features;
    msg.rate = 44100;
    msg.samples = 1024;

    msg.has_audio_version = 1;
    msg.has_feature_version = 1;
    msg.has_rate = 1;
    msg.has_samples = 1;

    return send_data_pb(buffer, sizeof(buffer), audio_data_fields, &msg);
}

bool encode_mac(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    unsigned char tagtype = (7 << 3) | 0x2; // field_number << 3 | 2 (length deliminated)
    unsigned char mac[6];
    unsigned char mac_len = 6;
#if 1
    mac[0] = 0xab;
    mac[1] = 0xcd;
    mac[2] = 0xab;
    mac[3] = 0xcd;
    mac[4] = 0xab;
    mac[5] = 0xcd;
#else
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
#endif
    return pb_write(stream, &tagtype, 1) && pb_encode_string(stream, (uint8_t*) mac, mac_len);
}

bool encode_name(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    unsigned char tagtype = (6 << 3) | 0x2; // field_number << 3 | 2 (length deliminated)
    return pb_write(stream, &tagtype, 1)
            && pb_encode_string(stream, (uint8_t*) MORPH_NAME,
                    strlen(MORPH_NAME));
}

int send_periodic_data( data_t * data ) {
    char buffer[256];
    periodic_data msg;

    //build the message
    msg.firmware_version = 2;
    msg.dust = data->dust;
    msg.humidity = data->humid;
    msg.light = data->light;
    msg.light_variability = data->light_variability;
    msg.light_tonality = data->light_tonality;
    msg.temperature = data->temp;
    msg.unix_time = data->time;
    msg.name.funcs.encode = encode_name;
    msg.mac.funcs.encode = encode_mac;

    msg.has_dust = 1;
    msg.has_humidity = 1;
    msg.has_light = 1;
    msg.has_temperature = 1;
    msg.has_unix_time = 1;

    return send_data_pb(buffer, sizeof(buffer), periodic_data_fields, &msg);
}


int Cmd_sl(int argc, char*argv[]) {
#define WLAN_DEL_ALL_PROFILES 0xff

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



#define MCU_VERSION    "MCU_01"
#ifdef _WIN32
#define VERSION_FROM_NWP_VER
#define OTA_VENDOR_STR "Vid01_Pid31_VerAA"  /* vendor id = 01, product id = 31, version will be filled online */
#else
#define OTA_VENDOR_STR "Vid01_Pid32_VerAA"  /* vendor id = 01, product id = 32, version will be filled online */
#endif

/* OTA server info */
/* --------------- */
#ifdef TI_OTA_SERVER
#define OTA_SERVER_NAME                 "10.0.0.13"
/*#define OTA_SERVER_NAME               "ild100069"" */
#define OTA_SERVER_IP_ADDRESS           0x0a00000d /*10.0.0.13 */
#define OTA_SERVER_SECURED              0
#define OTA_SERVER_REST_UPDATE_CHK      "/api/1/ota/update_check/vid:100" /* returns files/folder list */
#define OTA_SERVER_REST_RSRC_METADATA   "/api/1/ota/1001/metadata"        /* returns files/folder list */
#define OTA_SERVER_REST_HDR             " Connection: "
#define OTA_SERVER_REST_HDR_VAL         "keep-alive"
#else
#define OTA_SERVER_NAME                 "api.dropbox.com"
#define OTA_SERVER_IP_ADDRESS           0x00000000
#define OTA_SERVER_SECURED              1
#define OTA_SERVER_REST_UPDATE_CHK      "/1/metadata/auto/" /* returns files/folder list */
#define OTA_SERVER_REST_RSRC_METADATA   "/1/media/auto"     /* returns A url that serves the media directly */
#define OTA_SERVER_REST_HDR             "Authorization: Bearer "
#define OTA_SERVER_REST_HDR_VAL         "uSERguwUj98AAAAAAAAAdm8Vc_S0M0FIYYjg3Q0VYAZLQ9r58luEVq4LVYYosykm"
#define LOG_SERVER_NAME                 "api-content.dropbox.com"
#define OTA_SERVER_REST_FILES_PUT       "/1/files_put/auto/"
#endif

/* local data */
void *pvOtaApp;
OtaOptServerInfo_t g_otaOptServerInfo; /* must be global, will be pointed by extlib_ota */

void get_mac_address(_u8 *pMacAddress)
{
    _u8 macAddressLen = SL_MAC_ADDR_LEN;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddressLen, pMacAddress);
}

void get_version_str(char *versionStr)
{
#ifdef VERSION_FROM_NWP_VER
    _u8  pConfigOpt;
    _u8  pConfigLen;
    SlVersionFull ver;

    pConfigOpt = SL_DEVICE_GENERAL_VERSION;
    pConfigLen = sizeof(ver);
    sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&pConfigOpt,&pConfigLen,(_u8  *)(&ver));

    /* use NWP service pack number */
    sprintf(versionStr, "%02d", ver.NwpVersion[3]);
#else
    /*#define MCU_VERSION    "MCU_15"     */
    versionStr[0] = MCU_VERSION[4];
    versionStr[1] = MCU_VERSION[5];
#endif
}

void print_version()
{
    _u8  pConfigOpt;
    _u8  pConfigLen;
    SlVersionFull ver;

    UARTprintf("----- MCU version: %s\n", MCU_VERSION);
    pConfigOpt = SL_DEVICE_GENERAL_VERSION;
    pConfigLen = sizeof(ver);
    sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&pConfigOpt,&pConfigLen,(_u8  *)(&ver));

    UARTprintf("CHIP %d\nMAC 31.%d.%d.%d.%d\nPHY %d.%d.%d.%d\nNWP %d.%d.%d.%d\nROM %d\nHOST %d.%d.%d.%d\n",
             ver.ChipFwAndPhyVersion.ChipId,
             ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
             ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
             ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
             ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3],
             ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
             ver.RomVersion,
             SL_MAJOR_VERSION_NUM,SL_MINOR_VERSION_NUM,SL_VERSION_NUM,SL_SUB_VERSION_NUM);
    sl_extLib_OtaGet(pvOtaApp, EXTLIB_OTA_GET_OPT_PRINT_STAT, NULL, NULL);
}

int proc_ota_init()
{
    UARTprintf("proc_ota: init\n");
    pvOtaApp = sl_extLib_OtaInit(RUN_MODE_NONE_OS | RUN_MODE_BLOCKING, NULL /* host storage callbacks, must be global */);
    if (pvOtaApp == NULL)
    {
        UARTprintf("proc_ota_init: ERROR from sl_extLib_OtaInit\n");
        return RUN_STAT_ERROR;
    }

    return RUN_STAT_OK;
}

int proc_ota_config(void)
{
    char vendorStr[20];

    /* set OTA server info */
    g_otaOptServerInfo.ip_address = OTA_SERVER_IP_ADDRESS;
    g_otaOptServerInfo.secured_connection = OTA_SERVER_SECURED;
    strcpy(g_otaOptServerInfo.server_domain, OTA_SERVER_NAME);
    strcpy(g_otaOptServerInfo.rest_update_chk, OTA_SERVER_REST_UPDATE_CHK);
    strcpy(g_otaOptServerInfo.rest_rsrc_metadata, OTA_SERVER_REST_RSRC_METADATA);
    strcpy(g_otaOptServerInfo.rest_hdr, OTA_SERVER_REST_HDR);
    strcpy(g_otaOptServerInfo.rest_hdr_val, OTA_SERVER_REST_HDR_VAL);
    strcpy(g_otaOptServerInfo.log_server_name, LOG_SERVER_NAME);
    strcpy(g_otaOptServerInfo.rest_files_put, OTA_SERVER_REST_FILES_PUT);
    get_mac_address(g_otaOptServerInfo.log_mac_address);
    sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_SERVER_INFO, sizeof(g_otaOptServerInfo), (char *)&g_otaOptServerInfo);

    /* set vendor ID */
    strcpy(vendorStr, OTA_VENDOR_STR);
    get_version_str(&vendorStr[15]);
    sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_VENDOR_ID, strlen(vendorStr), vendorStr);

    return RUN_STAT_OK;
}

int proc_ota_run_step()
{
    int status;
    int SetCommitInt = 1;
    char vendorStr[20];

    /*UARTprintf("proc_ota: call sl_extLib_OtaRun\n"); */
    status = sl_extLib_OtaRun(pvOtaApp);
    if (status < 0)
    {
        if (status == RUN_STAT_ERROR_CONTINUOUS_ACCESS_FAILURES)
        {
            /* ACCESS Error - should reset the NWP */
            UARTprintf("proc_ota_run_step: ACCESS ERROR from sl_extLib_OtaRun %d !!!!!!!!!!!!!!!!!!!!!!!!!!!\n", status);

            nwp_reset();
        }
        else
        {
            /* OTA will go back to IDLE and next sl_extLib_OtaRun will restart the process */
            UARTprintf("proc_ota_run_step: ERROR from sl_extLib_OtaRun %d\n", status);
        }
    }
    else if (status == RUN_STAT_NO_UPDATES)
    {
        /* OTA will go back to IDLE and next sl_extLib_OtaRun will restart the process */
        UARTprintf("proc_ota_run_step: status from sl_extLib_OtaRun: no updates, wait for next period\n");
    }
    else if (status == RUN_STAT_DOWNLOAD_DONE)
    {
        UARTprintf("proc_ota_run_step: status from sl_extLib_OtaRun: Download done, status = 0x%x\n", status);
        print_version();

        /* Check and test the new MCU/NWP image (if exists) */
        /* The image test, in case of new MCU image, will include instruction to the MCU bootloader to switch images in order to test the new image */
        /* The host should do the MCU reset in case of status from image test will be set */
        status = sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_TEST, 0, 0);
        if (status & OTA_ACTION_RESET_MCU)
        {
            /* reset the MCU in order to test the new image */
            UARTprintf("\nproc_ota_run_step: reset the MCU to test the new image ---\n");
            UARTprintf("proc_ota_run_step: Rebooting...\n\n");
            mcu_reset();
            /*
             * if we reach here, the platform does not support self reset
            */
            UARTprintf("proc_ota_run_step: return without MCU reset\n");
        }
        if (status & OTA_ACTION_RESET_NWP)
        {
            /* reset the NWP in order to test the new image */
            UARTprintf("\nproc_ota_run_step: reset the NWP to test the new image ---\n");
            nwp_reset();

            if( status < 0 )
            {
                SetCommitInt = 0;
                UARTprintf("proc_ota_run_step: ERROR from slAccess_start_and_connect!!!!!!!!!!!!!\n");
                /* Reset #2 - should in next versions to do NWP rollback */
                status = sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_COMMIT, sizeof(int), (char *)&SetCommitInt);
                if (status & OTA_ACTION_RESET_NWP)
                {
                    UARTprintf("\n!!!!!!!!!!!!!!!!!!!!!!proc_ota_run_step: reset #2 the NWP to test the new image ---\n");
                    nwp_reset();
                }
            }
        }
        /* Set OTA commited flag - this ends the current OTA process, will be rerstart on next period */
        SetCommitInt = 1;
        sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_COMMIT, sizeof(int), (char *)&SetCommitInt);
        /* set vendor ID - for next period check */
        strcpy(vendorStr, OTA_VENDOR_STR);
        get_version_str(&vendorStr[15]);
        sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_VENDOR_ID, strlen(vendorStr), vendorStr);
    }
    else
    {
        /*UARTprintf("proc_ota_run_step: status from sl_extLib_OtaRun %d, default continue\n", status); */
    }
    return 0;
}

int proc_ota_is_active()
{
    int ProcActive;
    int ProcActiveLen;

    /* check OTA process */
    sl_extLib_OtaGet(pvOtaApp, EXTLIB_OTA_GET_OPT_IS_ACTIVE, (signed long *)&ProcActiveLen, (char *)&ProcActive);

    return ProcActive;
}

#include "FreeRTOS.h"
#include "task.h"
void thread_ota(void * unused)
{
#define TEST_OTA 1
#if TEST_OTA
    int status, imageCommitFlag;

    /* Init OTA process */
	proc_ota_init();

	if (sl_mode != ROLE_STA) {
		/* If the MCU image is under test, the ImageCommit process will commit the new image and might reset the MCU */
		/* NWP commit process is not implemented yet */
		imageCommitFlag = OTA_ACTION_IMAGE_COMMITED;
		status = sl_extLib_OtaSet(pvOtaApp, EXTLIB_OTA_SET_OPT_IMAGE_COMMIT, sizeof(imageCommitFlag), (char *) &imageCommitFlag);
		if (status == OTA_ACTION_RESET_MCU) {
			UARTprintf("main: OTA_ACTION_RESET_MCU reset the platform to commit the new image\n");
			UARTprintf("main: Rebooting...\n\n");
			mcu_reset();
		}
	}

    /* Config OTA process - after net connection */
    proc_ota_config();
    print_version();

    /* noneOS process sharing loop */
    while(1)
    {

    	if( sl_status & HAS_IP ) {
			/* Run OTA process */
			status = proc_ota_run_step();

			/* Check if to sleep or to continue serve the processes */
			if (!proc_ota_is_active())
			{
				UARTprintf("\r\nOTA task sleeping, MCU version=%s\r\n", MCU_VERSION);
				vTaskDelay(60*60*1000);
			}
    	} else {
			UARTprintf("\r\nOTA task sleeping waiting for wifi, MCU version=%s\r\n", MCU_VERSION);
			vTaskDelay(60*60*1000);
    	}
    }
#endif
}
#include "fatfs_cmd.h"

int audio_read_fn (char * buffer, int buffer_size) {
	//read audio file from fs
	WORD read;
	FRESULT res;

    res = f_read(&file_obj,buffer, buffer_size, &read);
    if( res != FR_OK ) {
    	UARTprintf( "f_read error %d", res);
    }
    UARTprintf( "read %d bytes", read);

	return read;
}

//*****************************************************************************
//
//! itoa
//!
//!    @brief  Convert integer to ASCII in decimal base
//!
//!     @param  cNum is input integer number to convert
//!     @param  cString is output string
//!
//!     @return number of ASCII parameters
//!
//!
//
//*****************************************************************************
const char pcDigits[] = "0123456789";
unsigned short itoa(char cNum, char *cString)
{
    char* ptr;
    char uTemp = cNum;
    unsigned short length;

    // value 0 is a special case
    if (cNum == 0)
    {
        length = 1;
        *cString = '0';

        return length;
    }

    // Find out the length of the number, in decimal base
    length = 0;
    while (uTemp > 0)
    {
        uTemp /= 10;
        length++;
    }

    // Do the actual formatting, right to left
    uTemp = cNum;
    ptr = cString + length;
    while (uTemp > 0)
    {
        --ptr;
        *ptr = pcDigits[uTemp % 10];
        uTemp /= 10;
    }

    return length;
}

unsigned long get_time();

int Cmd_audio_test(int argc, char *argv[]) {
#define AUDIO_WINDOW_LEN 1024
#define AUDIO_DIR "/audio/"
#define FULL_FN_LEN 48
#define FN_LEN 32

	//get the data
	short audio[AUDIO_WINDOW_LEN];
	char fn[FN_LEN];
	char full_fn[FULL_FN_LEN];
	//int fn_len;
	FRESULT res;
	int time = get_time();

	/*fn_len =*/ itoa(time, fn);
	strncat( full_fn, AUDIO_DIR, FULL_FN_LEN );
	strncat( full_fn, fn, FULL_FN_LEN );

    #define AUDIO_DIR "/audio/"
	res = f_open(&file_obj, full_fn, FA_CREATE_NEW|FA_WRITE);
    if( res != FR_OK ) {
    	UARTprintf( "f_open error %s %d", full_fn, res);
    }

    //record 10 seconds to file
	while( get_time() - time < 10 ) {
		WORD bytes_written = 0;
		WORD bytes_to_write = AUDIO_WINDOW_LEN;
		WORD bytes = 0;
		get_audio( audio, AUDIO_WINDOW_LEN );
		//todo pass windows to preclassifier

		do {
			res = f_write( &file_obj, audio+bytes_written, bytes_to_write-bytes_written, &bytes );
			bytes_written+=bytes;
		} while( bytes_written < bytes_to_write );
	}
    //todo check result of preclassifier

	//was it interesting?
    //seek back to beginning and transmit
    res = f_lseek(&file_obj, 0);
    send_audio_wifi( (char*)audio, sizeof(audio), audio_read_fn, time );
	//if not, delete it
	//else send
	//then delete

    res = f_close( &file_obj );
    if( res != FR_OK ) {
    	UARTprintf( "f_close error %s %d", full_fn, res);
    }
    res = f_unlink( full_fn );
    if( res != FR_OK ) {
    	UARTprintf( "f_unlink error %s %d", full_fn, res);
    }

//	send_audio_wifi( (char*)audio, sizeof(audio), audio_read_fn, time );
	//TODO: send through filesystem....
	//send_audio_wifi( (char*)audio, sizeof(audio), audio_input_fn, time );

	//read it from the file and send it to the server in chunks
	return (0);
}

