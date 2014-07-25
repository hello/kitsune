#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "uartstdio.h"

#include "simplelink.h"
#include "protocol.h"
#include "wifi_cmd.h"

#define ROLE_INVALID (-5)

int sl_mode = ROLE_INVALID;

unsigned int sl_status = 0;

//*****************************************************************************
//
//! This function gets triggered when HTTP Server receives Application
//! defined GET and POST HTTP Tokens.
//!
//! \param pHttpServerEvent Pointer indicating http server event
//! \param pHttpServerResponse Pointer indicating http server response
//!
//! \return None
//!
//*****************************************************************************
void sl_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent,
        SlHttpServerResponse_t *pSlHttpServerResponse) {

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
void sl_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent) {
    switch (pSlWlanEvent->Event) {
    case SL_WLAN_SMART_CONFIG_START_EVENT:
        /* SmartConfig operation finished */
        /*The new SSID that was acquired is: pWlanEventHandler->EventData.smartConfigStartResponse.ssid */
        /* We have the possiblity that also a private token was sent to the Host:
         *  if (pWlanEventHandler->EventData.smartConfigStartResponse.private_token_len)
         *    then the private token is populated: pWlanEventHandler->EventData.smartConfigStartResponse.private_token
         */
        UARTprintf("SL_WLAN_SMART_CONFIG_START_EVENT\n\r");
        break;
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
void sl_NetAppEvtHdlr(SlNetAppEvent_t *pNetAppEvent) {

    switch (pNetAppEvent->Event) {
    case SL_NETAPP_IPV4_ACQUIRED:
    case SL_NETAPP_IPV6_ACQUIRED:
        UARTprintf("SL_NETAPP_IPV4_ACQUIRED\n\r");
        sl_status |= HAS_IP;
        break;

    case SL_NETAPP_IP_LEASED:
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

    secParams.Key = argv[2];
    secParams.KeyLen = strlen(argv[2]);
    secParams.Type = atoi(argv[3]);

    sl_WlanConnect(argv[1], strlen(argv[1]), "123456", &secParams, 0);
    return (0);
}

int Cmd_status(int argc, char *argv[]) {
    unsigned char ucDHCP = 0;
    unsigned char len = sizeof(_NetCfgIpV4Args_t);
    //
    // Get IP address
    //    unsigned char len = sizeof(_NetCfgIpV4Args_t);
    _NetCfgIpV4Args_t ipv4 = { 0 };

    sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO, &ucDHCP, &len,
            (unsigned char *) &ipv4);
    //
    // Send the information
    //
    UARTprintf("%x ip 0x%x submask 0x%x gateway 0x%x dns 0x%x\n\r", sl_status, ipv4.ipV4,
            ipv4.ipV4Mask, ipv4.ipV4Gateway, ipv4.ipV4DnsServer);
    return 0;
}

// callback routine
void pingRes(SlPingReport_t* pReport) {
    // handle ping results
    UARTprintf(
            "Ping tx:%d rx:%d min time:%d max time:%d avg time:%d test time:%d\n",
            pReport->PacketsSent, pReport->PacketsReceived,
            pReport->MinRoundTime, pReport->MaxRoundTime, pReport->AvgRoundTime,
            pReport->TestTime);
}

int Cmd_ping(int argc, char *argv[]) {
    static SlPingReport_t report;
    SlPingStartCommand_t pingCommand;

    pingCommand.Ip = SL_IPV4_VAL(192, 3, 116, 75); // destination IP address is my router's IP
    pingCommand.PingSize = 32;                     // size of ping, in bytes
    pingCommand.PingIntervalTime = 100;   // delay between pings, in miliseconds
    pingCommand.PingRequestTimeout = 1000; // timeout for every ping in miliseconds
    pingCommand.TotalNumberOfAttempts = 3; // max number of ping requests. 0 - forever
    pingCommand.Flags = 1;                        // report after each ping

    sl_NetAppPingStart(&pingCommand, SL_AF_INET, &report, pingRes);
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
        return -1;
    }
    UARTprintf("Socket created\n\r");

//
    // Send a query ? to the NTP server to get the NTP time
    //
    memset(buffer, 0, sizeof(buffer));

#define NTP_SERVER "pool.ntp.org"
    if (!(rv = gethostbyname(NTP_SERVER, strlen(NTP_SERVER),
            &ipaddr, AF_INET))) {
        UARTprintf(
                "Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
                NTP_SERVER, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
                SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
    } else {
        UARTprintf("failed to resolve ntp addr rv %d\n", rv);
        close(sock);
        return -1;
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
    rv = sendto(sock, buffer, sizeof(buffer), 0, &sAddr,
            sizeof(sAddr));
    if (rv != sizeof(buffer)) {
        UARTprintf("Could not send SNTP request\n\r\n\r");
        close(sock);
        return -1;    // could not send SNTP request
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

    rv = recvfrom(sock, buffer, sizeof(buffer), 0,
            (SlSockAddr_t *) &sLocalAddr, (SlSocklen_t*) &iAddrSize);
    if (rv <= 0) {
        UARTprintf("Did not receive\n\r");
        close(sock);
        return -1;
    }

    //
    // Confirm that the MODE is 4 --> server
    if ((buffer[0] & 0x7) != 4)    // expect only server response
            {
        UARTprintf("Expecting response from Server Only!\n\r");
        close(sock);
        return -1;    // MODE is not server, abort
    } else {
        char iIndex;

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
    return (unsigned long)ntp;
}

int Cmd_time(int argc, char*argv[]) {

    UARTprintf(" time is %u \n", unix_time());

    return 0;
}

int Cmd_mode(int argc, char*argv[]) {
    #define SL_STOP_TIMEOUT                 (30)
    int ap = 0;
    if(argc != 2 ) {
        UARTprintf("mode <1=ap 0=station>\n");
    }
    ap = atoi(argv[1]);
    if( ap && sl_mode != ROLE_AP ) {
        //Switch to AP Mode
        sl_WlanSetMode(ROLE_AP);
        sl_Stop(SL_STOP_TIMEOUT);
        sl_mode =  sl_Start(NULL,NULL,NULL);
    }
    if( !ap && sl_mode != ROLE_STA ) {
        //Switch to STA Mode
        sl_WlanSetMode(ROLE_STA);
        sl_Stop(SL_STOP_TIMEOUT);
        sl_mode =  sl_Start(NULL,NULL,NULL);
    }

    return 0;
}

/* protobuf includes */
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "periodic.pb.h"


bool encode_name(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    return pb_write(stream, (uint8_t*)MORPH_NAME, strlen(MORPH_NAME));
}

int send_data_pb ( data_t * data) {
    char buffer[256];

    int rv = 0;
    sockaddr sAddr;
    sockaddr_in sLocalAddr;
    int iAddrSize;
    unsigned long ipaddr;
    int sock;
    int numbytes = 0;

    timeval tv;

    /* This is the buffer where we will store our message. */
    uint8_t pb_buffer[128];
    size_t message_length;
    bool status;

    periodic_data msg;

    int send_length;

    /* Create a stream that will write to our buffer. */
    pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));

    msg.dust = 1011;// data->dust;
    msg.humidity = 789;// data->humid;
    msg.light = 456;//data->light;
    msg.temperature = 123;//data->temp;
    msg.unix_time = 1406060608;//data->time;
    msg.name.funcs.encode = encode_name;

    msg.has_dust = 1;
    msg.has_humidity = 1;
    msg.has_light = 1;
    msg.has_temperature = 1;
    msg.has_unix_time = 1;

    /* Now we are ready to encode the message! */
    status = pb_encode(&stream, periodic_data_fields, &msg);
    message_length = stream.bytes_written;

    snprintf( buffer, sizeof(buffer),
            "POST /in/morpheus/pb HTTP/1.1\r\n"
            "Host: in.skeletor.com\r\n"
            "Content-type: application/x-protobuf\r\n"
            "Content-length: %d\r\n"
            "\r\n",
            message_length
            );
    send_length = message_length + strlen(buffer);

    memmove( buffer+strlen(buffer), pb_buffer, message_length );

    /* Then just check for any errors.. */
    if (!status) {
        UARTprintf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    tv.tv_sec = 2;             // Seconds
    tv.tv_usec = 0;             // Microseconds. 10000 microseconds resolution
    setsockopt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

    if (sock < 0) {
        UARTprintf("Socket create failed\n\r");
        return -1;
    }
    UARTprintf("Socket created\n\r");

    #define DATA_SERVER "in.skeletor.com"
#if !LOCAL_TEST
    if (!(rv = gethostbyname(DATA_SERVER, strlen(DATA_SERVER),
            &ipaddr, SL_AF_INET))) {
        UARTprintf(
                "Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
                DATA_SERVER, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
                SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
    } else {
        UARTprintf("failed to resolve ntp addr rv %d\n");
        return -1;
    }

    sAddr.sa_family = AF_INET;
    // the source port
    sAddr.sa_data[0] = 0;//0xf;
    sAddr.sa_data[1] = 80;//0xa0; //4k
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

    UARTprintf("Connecting \n\r\n\r");
    if(rv = connect(sock, &sAddr, sizeof(sAddr))) {
        UARTprintf("Could not connect %d\n\r\n\r", rv);
        return -1;    // could not send SNTP request
    }


    UARTprintf("Sending request\n\r%s\n\r", buffer);
    numbytes = 0;
//    while( numbytes < strlen(buffer) ) {
        rv = send(sock, buffer, send_length, 0);
        numbytes += rv;
        UARTprintf("sent %d\n\r\n\r", rv);
        if (rv < 0) {
            return -1;
        }
//    }
    memset(buffer, 0, sizeof(buffer));

    UARTprintf("Waiting for reply\n\r\n\r");
    numbytes = 0;
//    while (numbytes < sizeof(buffer)) {
        rv = recv(sock, buffer, sizeof(buffer), 0);
        numbytes += rv;
        UARTprintf("recv %d\n\r\n\r", rv);
        if (rv <= 0) {
            return -1;
        }
//    }


    UARTprintf("Reply is:\n\r\n\r");
    buffer[127] = 0; //make sure it terminates..
    UARTprintf( "%s", buffer );

    close( sock );

    return 0;
}

#define MSG_VER 2
int send_data( data_t * data ) {

    char buffer[256];
    char datastr[256];
#if 1
    unsigned char mac[6];
    unsigned char mac_len;
    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL,&mac_len, mac);
    snprintf( datastr, sizeof(datastr),
                "%d,%d,%d,%d,%d,%d,%x%x%x%x%x%x,%s",
                MSG_VER,
                data->time,
                data->light,
                data->temp,
                data->humid,
                data->dust,
                mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],
                MORPH_NAME
                );
#else
    snprintf( datastr, sizeof(datastr),
                "%d,%d,%d,%d,%d,%d,%s",
                MSG_VER,
                data->time,
                data->light,
                data->temp,
                data->humid,
                data->dust,
                MORPH_NAME
                );
#endif

    snprintf( buffer, sizeof(buffer),
            "POST /in/morpheus HTTP/1.1\r\n"
            "Host: in.skeletor.com\r\n"
            "Content-type: text/plain\r\n"
            "Content-length: %d\r\n"
            "\r\n"
            "%s\r\n",
            strlen(datastr), datastr
            );

    int rv = 0;
    sockaddr sAddr;
    sockaddr_in sLocalAddr;
    int iAddrSize;
    unsigned long ipaddr;
    int sock;
    int numbytes = 0;

    timeval tv;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    tv.tv_sec = 2;             // Seconds
    tv.tv_usec = 0;             // Microseconds. 10000 microseconds resolution
    setsockopt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

    if (sock < 0) {
        UARTprintf("Socket create failed %d\n\r", sock);
        return -1;
    }
    UARTprintf("Socket created\n\r");

    #define DATA_SERVER "in.skeletor.com"
    if (!(rv = gethostbyname(DATA_SERVER, strlen(DATA_SERVER),
            &ipaddr, SL_AF_INET))) {
        UARTprintf(
                "Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
                DATA_SERVER, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
                SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
    } else {
        UARTprintf("failed to resolve ntp addr rv %d\n");
        return -1;
    }

    sAddr.sa_family = AF_INET;
    // the source port
    sAddr.sa_data[0] = 0x00;
    sAddr.sa_data[1] = 80;
    sAddr.sa_data[2] = (char) ((ipaddr >> 24) & 0xff);
    sAddr.sa_data[3] = (char) ((ipaddr >> 16) & 0xff);
    sAddr.sa_data[4] = (char) ((ipaddr >> 8) & 0xff);
    sAddr.sa_data[5] = (char) (ipaddr & 0xff);

    UARTprintf("Connecting \n\r\n\r");
    if(rv = connect(sock, &sAddr, sizeof(sAddr))) {
        UARTprintf("Could not connect %d\n\r\n\r", rv);
        return -1;    // could not send SNTP request
    }

    UARTprintf("Sending request\n\r%s\n\r", buffer);
    numbytes = 0;
//    while( numbytes < strlen(buffer) ) {
        rv = send(sock, buffer, strlen(buffer), 0);
        numbytes += rv;
        UARTprintf("sent %d\n\r\n\r", rv);
        if (rv < 0) {
            return -1;
        }
//    }
    memset(buffer, 0, sizeof(buffer));

    UARTprintf("Waiting for reply\n\r\n\r");
    numbytes = 0;
//    while (numbytes < sizeof(buffer)) {
        rv = recv(sock, buffer, sizeof(buffer), 0);
        numbytes += rv;
        UARTprintf("recv %d\n\r\n\r", rv);
        if (rv <= 0) {
            return -1;
        }
//    }


    UARTprintf("Reply is:\n\r\n\r");
    buffer[127] = 0; //make sure it terminates..
    UARTprintf( "%s", buffer );

    close( sock );

    return 0;
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
    sl_WlanPolicySet( SL_POLICY_CONNECTION,
            SL_CONNECTION_POLICY(1, 0, 0, 0, 0), &policyVal,
            1 /*PolicyValLen*/);

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
