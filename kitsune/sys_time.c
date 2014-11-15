#include "sys_time.h"
#include "wifi_cmd.h"

static unsigned long last_ntp = 0;
uint64_t get_cache_time()
{
    if(!last_ntp)
    {
        return 0;
    }

    SlDateTime_t dt =  {0};
    uint8_t configLen = sizeof(SlDateTime_t);
    uint8_t configOpt = SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
    int32_t ret = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION,&configOpt, &configLen,(_u8 *)(&dt));
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
unsigned long get_time() {
    portTickType now = xTaskGetTickCount();
    unsigned long ntp = 0;
    unsigned int tries = 0;

    if (last_ntp == 0) {

        while (ntp == 0) {
            UARTprintf("Get NTP time\n");
            networktask_enter_critical_region();
            while (!(sl_status & HAS_IP)) {
                vTaskDelay(100);
            } //wait for a connection the first time...

            ntp = last_ntp = unix_time();

            vTaskDelay((1 << tries) * 1000);
            if (tries++ > 5) {
                tries = 5;
            }

            if( ntp != 0 ) {
                SlDateTime_t tm;
                untime( ntp, &tm );
                UARTprintf( "setting sl time %d:%d:%d day %d mon %d yr %d", tm.sl_tm_hour,tm.sl_tm_min,tm.sl_tm_sec,tm.sl_tm_day,tm.sl_tm_mon,tm.sl_tm_year);

                sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime_t),(unsigned char *)(&tm));
            }
            networktask_exit_critical_region();
            UARTprintf("Get NTP time done\n");

        }

    } else if (last_ntp != 0) {
        ntp = get_cache_time();
    }
    return ntp;
}