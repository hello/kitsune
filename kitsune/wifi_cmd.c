
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <simplelink.h>

#include "netapp.h"
#include "uartstdio.h"

int Cmd_connect(int argc, char *argv[])
{
	if( argc == 2 ) {
		ConnectUsingSSID(argv[1]);
	} else {
	    UARTprintf("\n\r\n\rPlease enter the AP details in the format specified\n\r");
	    UARTprintf("Command Usage\n\r");
	    UARTprintf("-------------\n\r");
	    UARTprintf("<ap_ssid>:<security_type>:<password>:<WEP_key_id>:\n\r\n\r");
	    UARTprintf("\n\r");
	    UARTprintf("Parameters \n\r");
	    UARTprintf("---------- \n\r");
	    UARTprintf("ap_ssid - ssid of the AP to be connected\n\r");
	    UARTprintf("security_type - values 1(for Open) or 2(for WEP) or 3(for WPA)\n\r");
	    UARTprintf("password - network password in case of 2(for WEP) or 3(for WPA)\n\r");
	    UARTprintf("WEP_key_id - key ID in case of 2(for WEP)\n\r");
	    UARTprintf("\n\r");
	    UARTprintf("For Ex. (OPEN AP) --> APSSID:1:::\n\r");
	    UARTprintf("For Ex. (WEP AP)  --> APSSID:2:password:1:\n\r");
	    UARTprintf("For Ex. (WPA AP)  --> APSSID:3:password::\n\r");
	    UARTprintf("\n\r");

	}
    return(0);
}

int Cmd_initsl(int argc, char *argv[])
{
	InitDriver();
    return(0);
}

int Cmd_deinitsl(int argc, char *argv[])
{
	DeInitDriver();
    return(0);
}

int Cmd_status(int argc, char *argv[])
{
	unsigned long ip,sm,gw,dns;
    //
    // Get IP address
    //
    IpConfigGet(&ip,&sm,&gw,&dns);
    //
    // Send the information
    //
    UARTprintf("ip 0x%x submask 0x%x gateway 0x%x dns 0x%x\n\r", ip,sm,gw,dns);

    return(0);
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
