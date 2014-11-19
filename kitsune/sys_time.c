#include "simplelink.h"
#include "sys_time.h"
#include "wifi_cmd.h"
#include "networktask.h"
#include "task.h"
#include "uartstdio.h"
#include "sl_sync_include_after_simplelink_header.h"

#define YEAR_TO_DAYS(y) ((y)*365 + (y)/4 - (y)/100 + (y)/400)
static int _init = 0;

static void untime(unsigned long unixtime, SlDateTime_t *tm)
{
    /* First take out the hour/minutes/seconds - this part is easy. */

    tm->sl_tm_sec = unixtime % 60;
    unixtime /= 60;

    tm->sl_tm_min = unixtime % 60;
    unixtime /= 60;

    tm->sl_tm_hour = unixtime % 24;
    unixtime /= 24;

    /* unixtime is now days since 01/01/1970 UTC
     * Rebaseline to the Common Era */

    unixtime += 719499;

    /* Roll forward looking for the year.  This could be done more efficiently
     * but this will do.  We have to start at 1969 because the year we calculate here
     * runs from March - so January and February 1970 will come out as 1969 here.
     */
    for (tm->sl_tm_year = 1969; unixtime > YEAR_TO_DAYS(tm->sl_tm_year + 1) + 30; tm->sl_tm_year++)
        ;

    /* OK we have our "year", so subtract off the days accounted for by full years. */
    unixtime -= YEAR_TO_DAYS(tm->sl_tm_year);

    /* unixtime is now number of days we are into the year (remembering that March 1
     * is the first day of the "year" still). */

    /* Roll forward looking for the month.  1 = March through to 12 = February. */
    for (tm->sl_tm_mon = 1; tm->sl_tm_mon < 12 && unixtime > 367*(tm->sl_tm_mon+1)/12; tm->sl_tm_mon++)
        ;

    /* Subtract off the days accounted for by full months */
    unixtime -= 367*tm->sl_tm_mon/12;

    /* unixtime is now number of days we are into the month */

    /* Adjust the month/year so that 1 = January, and years start where we
     * usually expect them to. */
    tm->sl_tm_mon += 2;
    if (tm->sl_tm_mon > 12)
    {
        tm->sl_tm_mon -= 12;
        tm->sl_tm_year++;
    }

    tm->sl_tm_day = unixtime;
}

uint32_t get_nwp_time()
{

    SlDateTime_t dt =  {0};
    uint8_t configLen = sizeof(SlDateTime_t);
    uint8_t configOpt = SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
    int32_t ret = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&configOpt, &configLen,(_u8 *)(&dt));
    if(ret != 0)
    {
        UARTprintf("sl_DevGet failed, err: %d\n", ret);
        return INVALID_SYS_TIME;
    }

    uint32_t ntp = dt.sl_tm_sec + dt.sl_tm_min*60 + dt.sl_tm_hour*3600 + dt.sl_tm_year_day*86400 +
                (dt.sl_tm_year-70)*31536000 + ((dt.sl_tm_year-69)/4)*86400 -
                ((dt.sl_tm_year-1)/100)*86400 + ((dt.sl_tm_year+299)/400)*86400 + 171398145;
    return ntp;
}

uint32_t set_nwp_time(uint32_t unix_timestamp_sec)
{
	if(unix_timestamp_sec > 0) {
		SlDateTime_t tm;
		untime(unix_timestamp_sec, &tm);
		UARTprintf( "setting sl time %d:%d:%d day %d mon %d yr %d", tm.sl_tm_hour,tm.sl_tm_min,tm.sl_tm_sec,tm.sl_tm_day,tm.sl_tm_mon,tm.sl_tm_year);

		int32_t ret = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
				  SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
				  sizeof(SlDateTime_t),(unsigned char *)(&tm));
		if(ret != 0)
		{
			return INVALID_SYS_TIME;
		}
	}

	return unix_timestamp_sec;
}

static uint32_t unix_time() {
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
        return INVALID_SYS_TIME;
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
        return INVALID_SYS_TIME;
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
        return INVALID_SYS_TIME;    // could not send SNTP request
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
        return INVALID_SYS_TIME;
    }

    //
    // Confirm that the MODE is 4 --> server
    if ((buffer[0] & 0x7) != 4)    // expect only server response
            {
        UARTprintf("Expecting response from Server Only!\n\r");
        close(sock);
        return INVALID_SYS_TIME;    // MODE is not server, abort
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
    return ntp;
}

int time_module_initialized()
{
	return _init == 1;
}

int init_time_module()
{
	_init = 1;
	return _init == 1;
}


/*
 * WARNING: DONOT use this function in protobuf encoding/decoding function if you want to send the protobuf
 * over network, it will deadlock the network task.
 * Use get_cache_time instead.
 */
uint32_t fetch_time_from_ntp_server() {
    uint32_t ntp = INVALID_SYS_TIME;

	UARTprintf("Get NTP time\n");

	while(1)
	{
		while (!(sl_status & HAS_IP)) {
			vTaskDelay(100);
		} //wait for a connection...
		ntp = unix_time();

		if(ntp != INVALID_SYS_TIME)
		{
			break;
		}
		vTaskDelay(10000);
	}

	UARTprintf("Get NTP time done\n");


	return ntp;

}
