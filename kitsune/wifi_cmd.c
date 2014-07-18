#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "uartstdio.h"

#include "simplelink.h"
#include "protocol.h"

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
//!	\brief This function handles WLAN events
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
		break;
	case SL_WLAN_DISCONNECT_EVENT:
		UARTprintf("SL_WLAN_DISCONNECT_EVENT\n\r");
		break;
	default:
		break;
	}
}

//****************************************************************************
//
//!	\brief This function handles events for IP address acquisition via DHCP
//!		   indication
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

	sl_WlanConnect(argv[1], strlen(argv[1]), 0, &secParams, 0);
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
	UARTprintf("ip 0x%x submask 0x%x gateway 0x%x dns 0x%x\n\r", ipv4.ipV4,
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
	char cDataBuf[48];
	int iRet = 0;
	SlSockAddr_t sAddr;
	SlSockAddrIn_t sLocalAddr;
	int iAddrSize;
	unsigned long ntp;
	unsigned long ntp_ip;
	int sock;

	SlTimeval_t tv;

	sock = sl_Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	tv.tv_sec = 2;             // Seconds
	tv.tv_usec = 0;             // Microseconds. 10000 microseconds resolution
	sl_SetSockOpt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

	if (sock < 0) {
		UARTprintf("Socket create failed\n\r");
		return -1;
	}
	UARTprintf("Socket created\n\r");

//
	// Send a query ? to the NTP server to get the NTP time
	//
	memset(cDataBuf, 0, sizeof(cDataBuf));

#define NTP_SERVER "pool.ntp.org"
	if (!(iRet = sl_NetAppDnsGetHostByName(NTP_SERVER, strlen(NTP_SERVER),
			&ntp_ip, SL_AF_INET))) {
		UARTprintf(
				"Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
				NTP_SERVER, SL_IPV4_BYTE(ntp_ip, 3), SL_IPV4_BYTE(ntp_ip, 2),
				SL_IPV4_BYTE(ntp_ip, 1), SL_IPV4_BYTE(ntp_ip, 0));
	} else {
		UARTprintf("failed to resolve ntp addr iRet %d\n");
	}

	sAddr.sa_family = AF_INET;
	// the source port
	sAddr.sa_data[0] = 0x00;
	sAddr.sa_data[1] = 0x7B;    // UDP port number for NTP is 123
	sAddr.sa_data[2] = (char) ((ntp_ip >> 24) & 0xff);
	sAddr.sa_data[3] = (char) ((ntp_ip >> 16) & 0xff);
	sAddr.sa_data[4] = (char) ((ntp_ip >> 8) & 0xff);
	sAddr.sa_data[5] = (char) (ntp_ip & 0xff);

	cDataBuf[0] = 0b11100011;   // LI, Version, Mode
	cDataBuf[1] = 0;     // Stratum, or type of clock
	cDataBuf[2] = 6;     // Polling Interval
	cDataBuf[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	cDataBuf[12] = 49;
	cDataBuf[13] = 0x4E;
	cDataBuf[14] = 49;
	cDataBuf[15] = 52;

	UARTprintf("Sending request\n\r\n\r");
	iRet = sl_SendTo(sock, cDataBuf, sizeof(cDataBuf), 0, &sAddr,
			sizeof(sAddr));
	if (iRet != sizeof(cDataBuf)) {
		UARTprintf("Could not send SNTP request\n\r\n\r");
		return -1;    // could not send SNTP request
	}

	//
	// Wait to receive the NTP time from the server
	//
	iAddrSize = sizeof(SlSockAddrIn_t);
	sLocalAddr.sin_family = SL_AF_INET;
	sLocalAddr.sin_port = 0;
	sLocalAddr.sin_addr.s_addr = 0;
	sl_Bind(sock, (SlSockAddr_t *) &sLocalAddr, iAddrSize);

	UARTprintf("receiving reply\n\r\n\r");

	iRet = sl_RecvFrom(sock, cDataBuf, sizeof(cDataBuf), 0,
			(SlSockAddr_t *) &sLocalAddr, (SlSocklen_t*) &iAddrSize);
	if (iRet <= 0) {
		UARTprintf("Did not receive\n\r");
		return -1;
	}

	//
	// Confirm that the MODE is 4 --> server
	if ((cDataBuf[0] & 0x7) != 4)    // expect only server response
			{
		UARTprintf("Expecting response from Server Only!\n\r");
		return -1;    // MODE is not server, abort
	} else {
		char iIndex;

		//
		// Getting the data from the Transmit Timestamp (seconds) field
		// This is the time at which the reply departed the
		// server for the client
		//
		ntp = cDataBuf[40];
		ntp <<= 8;
		ntp += cDataBuf[41];
		ntp <<= 8;
		ntp += cDataBuf[42];
		ntp <<= 8;
		ntp += cDataBuf[43];

		ntp -= 2208988800UL;

		close(sock);
	}
	return ntp;
}

int Cmd_time(int argc, char*argv[]) {

	UARTprintf(" time is %u \n", unix_time());

	return 0;
}

int Cmd_skeletor(int argc, char*argv[]) {

	//send a test command to skeletor!

	return 0;
}
