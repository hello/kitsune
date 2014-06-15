
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
void sl_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent, SlHttpServerResponse_t *pSlHttpServerResponse)
{

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
void sl_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent)
{
  switch(pSlWlanEvent->Event)
  {
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
void sl_NetAppEvtHdlr(SlNetAppEvent_t *pNetAppEvent)
{

  switch( pNetAppEvent->Event )
  {
    case SL_NETAPP_IPV4_ACQUIRED:
	case SL_NETAPP_IPV6_ACQUIRED:
      UARTprintf("SL_NETAPP_IPV4_ACQUIRED\n\r");
      break;
    default:
      break;
  }
}

int Cmd_connect(int argc, char *argv[])
{
   SlSecParams_t secParams;

   if(argc!= 4) {
	   UARTprintf("usage: connect <ssid> <key> <security: 0=open, 1=wep, 2=wpa>\n\r");
   }

   secParams.Key = argv[2];
   secParams.KeyLen = strlen(argv[2]);
   secParams.Type = atoi(argv[3]);

   sl_WlanConnect(argv[1], strlen(argv[1]), 0, &secParams, 0);
   return(0);
}

void InitCallback(unsigned long Status) {
	UARTprintf( "sl_Start returns status %u\n\r", Status );
}

int Cmd_initsl(int argc, char *argv[])
{
    sl_Start(NULL,NULL,InitCallback);
	return 0;
}

int Cmd_deinitsl(int argc, char *argv[])
{
	sl_Stop(0);
	return 0;
}

int Cmd_status(int argc, char *argv[])
{
    unsigned char ucDHCP = 0;
    unsigned char len = sizeof(_NetCfgIpV4Args_t);
    //
    // Get IP address
    //    unsigned char len = sizeof(_NetCfgIpV4Args_t);
	_NetCfgIpV4Args_t ipv4 = {0};

	sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO,&ucDHCP,&len,(unsigned char *)&ipv4);
    //
    // Send the information
    //
    UARTprintf("ip 0x%x submask 0x%x gateway 0x%x dns 0x%x\n\r", ipv4.ipV4,ipv4.ipV4Mask,ipv4.ipV4Gateway,ipv4.ipV4DnsServer);
	return 0;
}

// callback routine
void pingRes(SlPingReport_t* pReport)
{
 // handle ping results
	UARTprintf( "Ping tx:%d rx:%d min time:%d max time:%d avg time:%d test time:%d",
			pReport->PacketsSent,
			pReport->PacketsReceived,
			pReport->MinRoundTime,
			pReport->MaxRoundTime,
			pReport->AvgRoundTime,
			pReport->TestTime);
}

int Cmd_ping(int argc, char *argv[])
{
   static SlPingReport_t report;
   SlPingStartCommand_t pingCommand;

   pingCommand.Ip = SL_IPV4_VAL(192,168,1,1);     // destination IP address is my router's IP
   pingCommand.PingSize = 32;                     // size of ping, in bytes
   pingCommand.PingIntervalTime = 100;           // delay between pings, in miliseconds
   pingCommand.PingRequestTimeout = 1000;        // timeout for every ping in miliseconds
   pingCommand.TotalNumberOfAttempts = 3;       // max number of ping requests. 0 - forever
   pingCommand.Flags = 1;                        // report after each ping

   sl_NetAppPingStart( &pingCommand, SL_AF_INET, &report, pingRes );
   return(0);
}

