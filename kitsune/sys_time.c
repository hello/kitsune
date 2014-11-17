#include "simplelink.h"
#include "sys_time.h"
#include "wifi_cmd.h"
#include "networktask.h"
#include "task.h"
#include "uartstdio.h"
#include "sl_sync_include_after_simplelink_header.h"

#define YEAR_TO_DAYS(y) ((y)*365 + (y)/4 - (y)/100 + (y)/400)
static unsigned long last_ntp = 0;

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

uint64_t get_cache_time()
{
    if(!last_ntp)
    {
        return 0;
    }

    SlDateTime_t dt =  {0};
    uint8_t configLen = sizeof(SlDateTime_t);
    uint8_t configOpt = SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
    int32_t ret = SL_SYNC(sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&configOpt, &configLen,(_u8 *)(&dt)));
    if(ret != 0)
    {
        UARTprintf("sl_DevGet failed, err: %d\n", ret);
        return 0;
    }

    uint64_t ntp = dt.sl_tm_sec + dt.sl_tm_min*60 + dt.sl_tm_hour*3600 + dt.sl_tm_year_day*86400 +
                (dt.sl_tm_year-70)*31536000 + ((dt.sl_tm_year-69)/4)*86400 -
                ((dt.sl_tm_year-1)/100)*86400 + ((dt.sl_tm_year+299)/400)*86400 + 171398145;
    return ntp;
}


/*
 * WARNING: DONOT use get_time in protobuf encoding/decoding function if you want to send the protobuf
 * over network, it will deadlock the network task.
 * Use get_cache_time instead.
 */
unsigned long get_time() {
    portTickType now = xTaskGetTickCount();
    unsigned int tries = 0;

	while (last_ntp == 0) {
		UARTprintf("Get NTP time\n");
		networktask_enter_critical_region();

		if (last_ntp != 0) {  // race condition: some other thread got the time.
			UARTprintf("Get NTP time done by other thread\n");
			networktask_exit_critical_region();
			return get_cache_time();
		}

		while (!(sl_status & HAS_IP)) {
			vTaskDelay(100);
		} //wait for a connection the first time...

		uint64_t ntp = unix_time();

		vTaskDelay((1 << tries) * 1000);
		if (tries++ > 5) {
			tries = 5;
		}

		if( ntp > 0 ) {
			SlDateTime_t tm;
			untime( ntp, &tm );
			UARTprintf( "setting sl time %d:%d:%d day %d mon %d yr %d", tm.sl_tm_hour,tm.sl_tm_min,tm.sl_tm_sec,tm.sl_tm_day,tm.sl_tm_mon,tm.sl_tm_year);

			SL_SYNC(sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
					  SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
					  sizeof(SlDateTime_t),(unsigned char *)(&tm)));
			last_ntp = ntp;   // Just be careful here, set last_ntp after setting the time to NWP, or there will be a race condition.
		}

		UARTprintf("Get NTP time done\n");
		networktask_exit_critical_region();

	}

	return get_cache_time();

}
