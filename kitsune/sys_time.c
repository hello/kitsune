#include "sys_time.h"
#include "wifi_cmd.h"
#include "networktask.h"
#include "task.h"
#include "uartstdio.h"

#include "i2c_if.h"
#include "uartstdio.h"
#include "i2c_cmd.h"

#include "time.h"

#include "sl_sync_include_after_simplelink_header.h"

static time_t cached_time;
static TickType_t cached_ticks;
static bool is_time_good = false;
extern xSemaphoreHandle i2c_smphr;

static int bcd_to_int( int bcd ) {
	int i=0;

	i += 10*((bcd & 0xf0)>>4);
	i += (bcd & 0xf);
	return i;
}
static int int_to_bcd( int i ) {
	int bcd = 0;

	bcd |= i%10;
	i/=10;
	bcd |= (i%10)<<4;
	return bcd;
}

#define FAILURE                 -1
#define SUCCESS                 0
#define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { LOGI( "fail at %s %d\n\r", __FILE__, __LINE__ ); return FAILURE;}
static int get_rtc_time( struct tm * dt ) {
	unsigned char data[7];
	unsigned char addy = 1;
	if (xSemaphoreTake(i2c_smphr, portMAX_DELAY)) {
		TRY_OR_GOTOFAIL(I2C_IF_Write(0x68, &addy, 1, 1));
		TRY_OR_GOTOFAIL(I2C_IF_Read(0x68, data, 7));
		xSemaphoreGive(i2c_smphr);
	}
	dt->tm_sec = bcd_to_int(data[0] & 0x7f);
	dt->tm_min = bcd_to_int(data[1] & 0x7f);
#if 0 //RTC can't be trusted...
	//sometimes there seems to be a partial failure and the years are wrong and the seconds are 0...
	if( dt->tm_sec != 0 && !(data[1] & 0x80) && !is_time_good ) {
		LOGI("Got time from RTC\n");
		is_time_good = true;
	}
#endif
	dt->tm_hour = bcd_to_int(data[2]);
	dt->tm_wday = bcd_to_int(data[3] & 0xf);
	dt->tm_mday = bcd_to_int(data[4]);
	dt->tm_mon = bcd_to_int(data[5] & 0x3f);
	dt->tm_year = bcd_to_int(data[6]) + 30;
	return 0;
}

static int set_rtc_time(struct tm * dt) {
	unsigned char data[8];

	data[0] = 1; //address to write to...
	data[1] = int_to_bcd(dt->tm_sec & 0x7f);
	data[2] = int_to_bcd(dt->tm_min & 0x7f); //sets the OF to FALSE
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

static time_t get_unix_time()
{
    struct tm dt =  {0};
    get_rtc_time(&dt);
    return mktime(&dt);
}

static uint32_t set_unix_time(time_t unix_timestamp_sec)
{
	if(unix_timestamp_sec > 0) {
		//set the RTC
		set_rtc_time(localtime(&unix_timestamp_sec));
	}
	return unix_timestamp_sec;
}

time_t get_sl_time() {
	SlDateTime_t sl_tm;
	struct tm dt;
	memset(&sl_tm, 0, sizeof(sl_tm));
	uint8_t cfg = SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
	uint8_t sz = sizeof(SlDateTime_t);
	sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &cfg, &sz,
			(unsigned char * )(&sl_tm));

	dt.tm_hour = sl_tm.sl_tm_hour;
	dt.tm_mday = sl_tm.sl_tm_day;
	dt.tm_hour = sl_tm.sl_tm_hour;
	dt.tm_min = sl_tm.sl_tm_min;
	dt.tm_mon = sl_tm.sl_tm_mon;
	dt.tm_mon = sl_tm.sl_tm_mon == 0 ? 12 : sl_tm.sl_tm_mon - 1;
	dt.tm_sec = sl_tm.sl_tm_sec;
	dt.tm_year = sl_tm.sl_tm_year - 1970;

	return mktime(&dt);
}
void set_sl_time(time_t unix_timestamp_sec) {
	SlDateTime_t sl_tm;

    struct tm * dt;
    dt = localtime(&unix_timestamp_sec);

    sl_tm.sl_tm_day = dt->tm_mday;
    sl_tm.sl_tm_hour = dt->tm_hour;
    sl_tm.sl_tm_min = dt->tm_min;
    sl_tm.sl_tm_mon = dt->tm_mon;
    sl_tm.sl_tm_mon+=1;
    sl_tm.sl_tm_mon = sl_tm.sl_tm_mon > 12 ? sl_tm.sl_tm_mon -= 12 : sl_tm.sl_tm_mon;
    sl_tm.sl_tm_sec = dt->tm_sec;
    sl_tm.sl_tm_year = dt->tm_year + 1970;

	sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
			  SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
			  sizeof(SlDateTime_t),(unsigned char *)(&sl_tm));
	memset(&sl_tm, 0, sizeof(sl_tm));
	uint8_t cfg = SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
	uint8_t sz = sizeof(SlDateTime_t);
	sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,
			  &cfg,
			  &sz,
			  (unsigned char *)(&sl_tm));

    LOGI("Day %d,Mon %d,Year %d,Hour %d,Min %d,Sec %d\n",sl_tm.sl_tm_day,sl_tm.sl_tm_mon,sl_tm.sl_tm_year, sl_tm.sl_tm_hour,sl_tm.sl_tm_min,sl_tm.sl_tm_sec);
}

uint32_t fetch_unix_time_from_ntp() {
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

#define NTP_SERVER "ntp.hello.is"
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

    rv = recvfrom(sock, buffer, sizeof(buffer), 0, (SlSockAddr_t *) &sLocalAddr,  (SlSocklen_t*) &iAddrSize);
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
    }
    close(sock);
    return ntp;
}

static xSemaphoreHandle time_smphr = NULL;

static void set_cached_time( time_t t ) {
	cached_time = t;
	cached_ticks = xTaskGetTickCount();
}
static time_t get_cached_time() {
	return  cached_time + (xTaskGetTickCount() - cached_ticks) / 1000;
}


static void time_task( void * params ) { //exists to get the time going and cache so we aren't going to NTP or RTC every time...
	#define TIME_POLL_INTERVAL 86400000ul //one DAY
	bool have_set_time = false;
	TickType_t last_set = 0;
	while (1) {
		if ((!have_set_time || xTaskGetTickCount()- last_set > TIME_POLL_INTERVAL )
			&& wifi_status_get(HAS_IP) && time_smphr && xSemaphoreTake(time_smphr, 0)) {
			uint32_t ntp_time = fetch_unix_time_from_ntp();
			if (ntp_time != INVALID_SYS_TIME) {
				if (set_unix_time(ntp_time) != INVALID_SYS_TIME) {
					is_time_good = true;
					set_cached_time(ntp_time);
					set_sl_time(get_cached_time());
					have_set_time = true;
					last_set = xTaskGetTickCount();
				}
			}
			xSemaphoreGive(time_smphr);
		}

		if (time_smphr && xSemaphoreTake(time_smphr, 0)) {
			if( !is_time_good || xTaskGetTickCount() - cached_ticks > 30000 ) {
				set_cached_time(get_unix_time());
			}
			xSemaphoreGive(time_smphr);
		}
		vTaskDelay(1000);
	}
}

bool wait_for_time(int sec) { //todo make event based, maybe use semaphore
	if(sec == WAIT_FOREVER)
	{
		while(!has_good_time()){
			vTaskDelay(1000);
		}
	}else{

		while(!has_good_time()){
			vTaskDelay(configTICK_RATE_HZ);
			if(--sec == 0)
			{
				return 0;
			}
		}
	}

	return 1;
}

bool has_good_time() {
	bool good = false;
	if (time_smphr) {
		if (xSemaphoreTake(time_smphr, 0)) {
			good = is_time_good;
			xSemaphoreGive(time_smphr);
		} else {
			good = false;
		}
	}
	return good;
}

time_t get_time() { //all accesses go to cache...
	time_t t = INVALID_SYS_TIME;
	if (time_smphr) {
		if (cached_time != INVALID_SYS_TIME && xSemaphoreTake(time_smphr, 0)) {
			t = get_cached_time();
			xSemaphoreGive(time_smphr);
		}
	}
	return t;
}

void init_time_module(int stack)
{
	is_time_good = false;
	cached_ticks = 0;
	cached_time = INVALID_SYS_TIME;
	vSemaphoreCreateBinary(time_smphr);
	xTaskCreate(time_task, "time_task", stack / 4, NULL, 4, NULL); //todo reduce stack
}
