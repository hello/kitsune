
#include "FreeRTOS.h"
#include "semphr.h"
#include "uart_logger.h"
#include "stdbool.h"

#include "sl_sync_include_after_simplelink_header.h"

static xSemaphoreHandle _sl_mutex;

extern volatile bool booted;

#include "SEGGER_SYSVIEW.h"
long sl_sync_init()
{
	_sl_mutex = xSemaphoreCreateRecursiveMutex();
	SEGGER_SYSVIEW_NameResource((U32)_sl_mutex, "SL SYNC SEM");
	return 1;
}

long sl_enter_critical_region()
{
    if( !xSemaphoreTakeRecursive(_sl_mutex, 60000) ) {
    	return 0;
    }
    return 1;
}

long sl_exit_critical_region()
{
    return xSemaphoreGiveRecursive(_sl_mutex);
}
