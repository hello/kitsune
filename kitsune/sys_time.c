#include "sys_time.h"
#include "wifi_cmd.h"
#include "networktask.h"
#include "task.h"
#include "uartstdio.h"

#include "i2c_if.h"
#include "uartstdio.h"
#include "i2c_cmd.h"

#include "kit_assert.h"
#include "time.h"

#include "ustdlib.h"
#include "socket.h"


#include "sl_sync_include_after_simplelink_header.h"

int send_top(char * s, int n) ;

static time_t cached_time;
static TickType_t cached_ticks;
static bool is_time_good = false;
extern xSemaphoreHandle i2c_smphr;
extern volatile bool booted;

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

static int get_rtc_time( struct tm * dt ) {
	unsigned char data[7];
	unsigned char addy = 1;
	int retries = 0;
	assert (xSemaphoreTakeRecursive(i2c_smphr, 300000));
	vTaskDelay(5);
	while (I2C_IF_Write(0x68, &addy, 1, 1) || I2C_IF_Read(0x68, data, 7)) {
		vTaskDelay(5);
		xSemaphoreGiveRecursive(i2c_smphr);
		vTaskDelay(1000);
		assert (xSemaphoreTakeRecursive(i2c_smphr, 300000));
		++retries;
	}
	xSemaphoreGiveRecursive(i2c_smphr);
	if( retries > 0 ) {
		LOGW("rtc i2c retries %d\n", retries);
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
	dt->tm_mon = bcd_to_int(data[5] & 0x3f)-1;
	dt->tm_year = bcd_to_int(data[6])+100;
	return 0;
}

static int set_rtc_time(struct tm * dt) {
	unsigned char data[8];
	int retries = 0;
	data[0] = 1; //address to write to...
	data[1] = int_to_bcd(dt->tm_sec & 0x7f);
	data[2] = int_to_bcd(dt->tm_min & 0x7f); //sets the OF to FALSE
	data[3] = int_to_bcd(dt->tm_hour);
	data[4] = int_to_bcd(dt->tm_wday)|0x10; //the first 4 bits here need to be 1 always
    data[5] = int_to_bcd(dt->tm_mday);
    data[6] = int_to_bcd((dt->tm_mon+1) & 0x3f );
    data[7] = int_to_bcd(dt->tm_year-100);

	if (xSemaphoreTakeRecursive(i2c_smphr, 300000)) {
		vTaskDelay(5);
		while (I2C_IF_Write(0x68, data, 8, 1) != 0) {
			vTaskDelay(5);
			++retries;
		};
		xSemaphoreGiveRecursive(i2c_smphr);
	} else {
		LOGW("failed to get i2c %d\n", __LINE__);
	}
	if( retries > 0 ) {
		LOGW("set rtc retry %d\n", retries);
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
	uint8_t cfg = SL_DEVICE_GENERAL_DATE_TIME;
	uint8_t sz = sizeof(SlDateTime_t);
    sl_DeviceGet(SL_DEVICE_GENERAL,&cfg, &sz,(_u8 *)(&sl_tm));

	dt.tm_mday = sl_tm.tm_day;
	dt.tm_hour = sl_tm.tm_hour;
	dt.tm_min = sl_tm.tm_min;
	dt.tm_mon = sl_tm.tm_mon-1;//coming from 1-12 going to 0-11, no rollover possible
	dt.tm_sec = sl_tm.tm_sec;
	dt.tm_year = sl_tm.tm_year - 1900;

    LOGI("OUT Day %d,Mon %d,Year %d,Hour %d,Min %d,Sec %d\n",sl_tm.tm_day,sl_tm.tm_mon,sl_tm.tm_year, sl_tm.tm_hour,sl_tm.tm_min,sl_tm.tm_sec);

	return mktime(&dt);
}
void set_sl_time(time_t unix_timestamp_sec) {
	SlDateTime_t sl_tm;

    struct tm * dt;
    dt = localtime(&unix_timestamp_sec);

    sl_tm.tm_day = dt->tm_mday;
    sl_tm.tm_hour = dt->tm_hour;
    sl_tm.tm_min = dt->tm_min;
    sl_tm.tm_mon = dt->tm_mon+1; //coming from 0-11 going to 1-12, no rollover possible
    sl_tm.tm_sec = dt->tm_sec;
    sl_tm.tm_year = dt->tm_year + 1900;

	_u8 persistent = 1;
	sl_DeviceSet(SL_DEVICE_GENERAL,
               SL_DEVICE_GENERAL_PERSISTENT,
               sizeof(_u8),
               (_u8 *)(&persistent));
    sl_DeviceSet(SL_DEVICE_GENERAL,
              SL_DEVICE_GENERAL_DATE_TIME,
              sizeof(SlDateTime_t),
              (_u8 *)(&sl_tm));


	memset(&sl_tm, 0, sizeof(sl_tm));
	uint8_t cfg = SL_DEVICE_GENERAL_DATE_TIME;
	uint8_t sz = sizeof(SlDateTime_t);
    sl_DeviceGet(SL_DEVICE_GENERAL,&cfg, &sz,(_u8 *)(&sl_tm));

    LOGI("IN Day %d,Mon %d,Year %d,Hour %d,Min %d,Sec %d\n",sl_tm.tm_day,sl_tm.tm_mon,sl_tm.tm_year, sl_tm.tm_hour,sl_tm.tm_min,sl_tm.tm_sec);
}


#include "ntp.pb.h"

#define TIME_HOST "time.hello.is"
#define TIME_ENDPOINT "/"

static int64_t last_reference_time = 0;
static int64_t current_ntp_time = 0;

static void _get_time_response(pb_field_t ** fields, void ** structdata){
	*fields = (pb_field_t *)hello_NTPDataPacket_fields;
	*structdata = pvPortMalloc(sizeof(hello_NTPDataPacket));
	assert(*structdata);
	if( *structdata ) {
		hello_NTPDataPacket * response_protobuf = *structdata;
		memset(response_protobuf, 0, sizeof(hello_NTPDataPacket));
	}
}
static void _free_time_response(void * structdata){
	vPortFree( structdata );
}

static void _on_time_response_success( void * structdata){
	hello_NTPDataPacket * time = structdata;
	LOGF("_on_time_response_success %u\r\n", (time->transmit_ts>>32) );
	if( time->has_transmit_ts && (time->transmit_ts>>32)!=0 ) { //todo use the received and origin timestamps
		current_ntp_time = last_reference_time = time->transmit_ts>>32;
	} else {
		LOGE("no or zero time!\n");
		current_ntp_time = INVALID_SYS_TIME;
	}
}
static void _on_time_response_failure( ){
	LOGF("_on_time_response_failure\r\n");
	current_ntp_time = INVALID_SYS_TIME;
}

uint32_t fetch_ntp_time_from_ntp() {
	#define MAX_RETRIES 30
	int retries = 0;
	int sock = -1;
	hello_NTPDataPacket request;
    protobuf_reply_callbacks pb_cb;

    while( !booted ) {
    	vTaskDelay(1000);
    }

	char * decode_buf = pvPortMalloc(SERVER_REPLY_BUFSZ);
    assert(decode_buf);
    memset(decode_buf, 0, SERVER_REPLY_BUFSZ);
    size_t decode_buf_size = SERVER_REPLY_BUFSZ;

    pb_cb.get_reply_pb = _get_time_response;
    pb_cb.free_reply_pb = _free_time_response;
    pb_cb.on_pb_success = _on_time_response_success;
    pb_cb.on_pb_failure = _on_time_response_failure;

    request.has_origin_ts = true;
    request.origin_ts = ((uint64_t)get_unix_time())<<31;
    request.has_reference_ts = true;
    request.reference_ts = last_reference_time<<32;

	while( send_data_pb(TIME_HOST,
			TIME_ENDPOINT,
			&decode_buf,
			&decode_buf_size,
			hello_NTPDataPacket_fields,
			&request,
			&pb_cb, &sock, SOCKET_SEC_NONE ) != 0 && ++retries < MAX_RETRIES ) {
		if( !is_time_good ) { //request top...
			//send_top("time", strlen("time"));
		}

		if( retries < 5 ) {
			vTaskDelay( (1<<retries)*1000 );
		} else {
			vTaskDelay( 32000 );
		}
	}
	if( sock > 0 ) {
		close(sock);
		sock = -1;
	}
	vPortFree(decode_buf);
    return current_ntp_time;
#undef MAX_RETRIES
}

static void set_cached_time( time_t t ) {
	cached_time = t;
	cached_ticks = xTaskGetTickCount();
}
static time_t get_cached_time() {
	return  cached_time + (xTaskGetTickCount() - cached_ticks) / 1000;
}

static xSemaphoreHandle time_smphr = NULL;

int cmd_set_time(int argc, char *argv[]) {
	if (time_smphr && xSemaphoreTake(time_smphr, portMAX_DELAY)) {
		set_cached_time(strtoul(argv[1],NULL,10));
		set_sl_time(get_cached_time());
		is_time_good = true;
		set_unix_time(get_cached_time());
		xSemaphoreGive(time_smphr);
	}

    LOGF("time is %u\n", get_time());
	return -1;
}

static void time_task( void * params ) { //exists to get the time going and cache so we aren't going to NTP or RTC every time...
	#define TIME_POLL_INTERVAL_MS (86400000ul>>3) //one eighth DAY
	#define TIME_POLL_INTERVAL_SEC (TIME_POLL_INTERVAL_MS/1000)
	#define MARGIN_OF_ERROR 800
	bool have_set_time = false;
	TickType_t last_set = 0;
	while (1) {
		if ((!have_set_time || xTaskGetTickCount()- last_set > TIME_POLL_INTERVAL_MS ) && wifi_status_get(HAS_IP) ) {
			uint32_t ntp_time = fetch_ntp_time_from_ntp();
			if (ntp_time != INVALID_SYS_TIME && time_smphr && xSemaphoreTake(time_smphr, 0) ) {
				if (set_unix_time(ntp_time) != INVALID_SYS_TIME) {
					char cmdbuf[20]={0};
					is_time_good = true;
					set_cached_time(ntp_time);
					set_sl_time(get_cached_time());
					have_set_time = true;
					last_set = xTaskGetTickCount();

					usnprintf( cmdbuf, sizeof(cmdbuf), "time %u\r\n", ntp_time);
					LOGI("sending %s\r\n", cmdbuf);
					//send_top(cmdbuf, strlen(cmdbuf));
				}
				xSemaphoreGive(time_smphr);
			}
		}
		if( !is_time_good ) { //request top...
			//send_top("time", strlen("time"));
			vTaskDelay(10000);
		}

		if (time_smphr && xSemaphoreTake(time_smphr, 0)) {
			if( xTaskGetTickCount() - cached_ticks > 30000 &&xSemaphoreTakeRecursive(i2c_smphr, 300000)) {
				set_cached_time(get_unix_time());
				xSemaphoreGiveRecursive(i2c_smphr);
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

uint32_t get_time() { //all accesses go to cache...
	uint32_t t = INVALID_SYS_TIME;
	if (time_smphr) {
		if (cached_time != INVALID_SYS_TIME && xSemaphoreTake(time_smphr, 0)) {
			t = get_cached_time();
			xSemaphoreGive(time_smphr);
		}
	}
	return t - 2208988800UL; //reset to 1970....
}

void init_time_module(int stack)
{
	is_time_good = false;
	cached_ticks = 0;
	cached_time = INVALID_SYS_TIME;
	vSemaphoreCreateBinary(time_smphr);
	set_cached_time(1422504361UL+2208988800UL); //default time gets us in jan 29th 2015
	set_sl_time(get_cached_time());
	xTaskCreate(time_task, "time_task", stack / 4, NULL, 4, NULL); //todo reduce stack
}

#include "limits.h"

/* Hardware library includes. */
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_wdt.h"
#include "wdt.h"
#include "wdt_if.h"
#include "rom.h"
#include "rom_map.h"

int Cmd_time_test(int argc, char * argv[]) {

#if 1 //test 4 times per day, does the RTC round trip correctly?
	{
	uint32_t i=3641861161UL,r;
	int cnt=0;
	while( i < 3641861161UL + 3641861161UL ) { //10 years
		vTaskDelay(10);
		set_unix_time(i);
		vTaskDelay(10);
		r =  get_unix_time();
		if( r != i ) {
			LOGE("TIME FAIL %u %u", i, r);
		}
		i+=3600*6;
		cnt++;
		MAP_WatchdogIntClear(WDT_BASE); //clear wdt
	}
}
#endif

#if 1 //test 4 times per day, does the RTC in the 3200 round trip correctly?
	{
	uint32_t i=3641861161UL,r;
	int cnt=0;
	while( i < 3641861161UL + 3641861161UL ) { //10 years
		vTaskDelay(10);
		set_sl_time(i);
		vTaskDelay(10);
		r =  get_sl_time();
		if( r != i ) {
			LOGE("TIME FAIL %u %u %d\n", i, r, i-r);
		}
		i+=3600*6;
		cnt++;
		MAP_WatchdogIntClear(WDT_BASE); //clear wdt
	}
}
#endif


#if 1 //do we roll across month endings correctly?
	{
	unsigned int mon_len[] =
		{31,28,31,30,31,30,31,31,30,31,30,31 };
	uint32_t i,r,y,m;
	y = 2015;
	m = 0;
	for(y=2015;y<2020;++y) {
		for(m=0;m<12;++m) {
			SlDateTime_t sl_tm;
			struct tm dt;
			memset(&sl_tm, 0, sizeof(sl_tm));
			uint8_t cfg = SL_DEVICE_GENERAL_DATE_TIME;
			uint8_t sz = sizeof(SlDateTime_t);
		    sl_DeviceGet(SL_DEVICE_GENERAL,&cfg, &sz,(_u8 *)(&sl_tm));

			dt.tm_mday = mon_len[m];
			dt.tm_hour = 23;
			dt.tm_min = 59;
			dt.tm_mon = m;
			dt.tm_sec = 59;
			dt.tm_year = y-1900;

			i =  mktime(&dt);
			set_unix_time( i );
			vTaskDelay(1100);
			r = get_unix_time();
			if( r != i + 1 ) {
				LOGE("ROLLOVER FAIL %d %d %u %u\n", m ,y, i, r);
			}
		}
	}
	}
#endif
	return 0;
}
