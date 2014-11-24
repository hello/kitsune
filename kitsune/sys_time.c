#include "sys_time.h"
#include "wifi_cmd.h"
#include "networktask.h"
#include "task.h"
#include "uartstdio.h"

#include "i2c_if.h"
#include "uartstdio.h"
#include "i2c_cmd.h"

#include "time.h"

static int _init = 0;
extern xSemaphoreHandle i2c_smphr;

int bcd_to_int( int bcd ) {
	int i=0;

	i += 10*((bcd & 0xf0)>>4);
	i += (bcd & 0xf);
	return i;
}
int int_to_bcd( int i ) {
	int bcd = 0;

	bcd |= i%10;
	i/=10;
	bcd |= (i%10)<<4;
	return bcd;
}

#define FAILURE                 -1
#define SUCCESS                 0
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { LOGI( "fail at %s %d\n\r", __FILE__, __LINE__ ); return FAILURE;}
int get_rtc_time( struct tm * dt ) {
	unsigned char data[7];
	unsigned char addy = 1;
	if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
		TRY_OR_GOTOFAIL(I2C_IF_Write(0x68, &addy, 1, 1));
		TRY_OR_GOTOFAIL(I2C_IF_Read(0x68, data, 7));
		xSemaphoreGive(i2c_smphr);
	}
	dt->tm_sec = bcd_to_int(data[0] & 0x7f);
	dt->tm_min = bcd_to_int(data[1] & 0x7f);
	dt->tm_hour = bcd_to_int(data[2]);
	dt->tm_wday = bcd_to_int(data[3] & 0xf);
	dt->tm_mday = bcd_to_int(data[4]);
	dt->tm_mon = bcd_to_int(data[5] & 0x3f);
	dt->tm_year = bcd_to_int(data[6]) + 30;
	return 0;
}

int set_rtc_time(struct tm * dt) {
	unsigned char data[8];

	data[0] = 1; //address to write to...
	data[1] = int_to_bcd(dt->tm_sec & 0x7f);
	data[2] = int_to_bcd(dt->tm_min & 0x7f);
	data[3] = int_to_bcd(dt->tm_hour);
	data[4] = int_to_bcd(dt->tm_wday)|0x10; //the first 4 bits here need to be 1 always
    data[5] = int_to_bcd(dt->tm_mday);
    data[6] = int_to_bcd(dt->tm_mon & 0x3f);
    data[7] = int_to_bcd(dt->tm_year - 30);

	if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
		TRY_OR_GOTOFAIL(I2C_IF_Write(0x68, data, 8, 1));
		xSemaphoreGive(i2c_smphr);
	}
	return 0;
}



time_t get_nwp_time()
{
    struct tm dt =  {0};
    get_rtc_time(&dt);
    return mktime(&dt);
}

uint32_t set_nwp_time(time_t unix_timestamp_sec)
{
	if(unix_timestamp_sec > 0) {
		//set the RTC
		set_rtc_time(localtime(&unix_timestamp_sec));
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
        LOGI("Socket create failed\n\r");
        return INVALID_SYS_TIME;
    }
    LOGI("Socket created\n\r");

//
    // Send a query ? to the NTP server to get the NTP time
    //
    memset(buffer, 0, sizeof(buffer));

#define NTP_SERVER "pool.ntp.org"
    if (!(rv = gethostbyname(NTP_SERVER, strlen(NTP_SERVER), &ipaddr, AF_INET))) {
        LOGI(
                "Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
                NTP_SERVER, SL_IPV4_BYTE(ipaddr, 3), SL_IPV4_BYTE(ipaddr, 2),
                SL_IPV4_BYTE(ipaddr, 1), SL_IPV4_BYTE(ipaddr, 0));
    } else {
        LOGI("failed to resolve ntp addr rv %d\n", rv);
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

    LOGI("Sending request\n\r\n\r");
    rv = sendto(sock, buffer, sizeof(buffer), 0, &sAddr, sizeof(sAddr));
    if (rv != sizeof(buffer)) {
        LOGI("Could not send SNTP request\n\r\n\r");
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

    LOGI("receiving reply\n\r\n\r");

    rv = recvfrom(sock, buffer, sizeof(buffer), 0, (SlSockAddr_t *) &sLocalAddr,
            (SlSocklen_t*) &iAddrSize);
    if (rv <= 0) {
        LOGI("Did not receive\n\r");
        close(sock);
        return INVALID_SYS_TIME;
    }

    //
    // Confirm that the MODE is 4 --> server
    if ((buffer[0] & 0x7) != 4)    // expect only server response
            {
        LOGI("Expecting response from Server Only!\n\r");
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

	LOGI("Get NTP time\n");

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

	LOGI("Get NTP time done\n");


	return ntp;

}
