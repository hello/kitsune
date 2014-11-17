#include "sl_sync.h"
#include "FreeRTOS.h"
#include "semphr.h"

static xSemaphoreHandle _sl_mutex;

long sl_sync_init()
{
	_sl_mutex = xSemaphoreCreateMutex();
	return 1;
}

long sl_enter_critical_region()
{
    return xSemaphoreTake(_sl_mutex, portMAX_DELAY);
}

long sl_exit_critical_region()
{
    return xSemaphoreGive(_sl_mutex);
}
