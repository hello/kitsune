
#include "FreeRTOS.h"
#include "semphr.h"

#include "sl_sync_include_after_simplelink_header.h"

static xSemaphoreHandle _sl_mutex;

long sl_sync_init()
{
	//_sl_mutex = xSemaphoreCreateMutex();
	vSemaphoreCreateBinary(_sl_mutex);
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
