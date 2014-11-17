#include "sl_sync.h"
#include "FreeRTOS.h"
#include "semphr.h"

static xSemaphoreHandle _sl_semaphore;

void sl_sync_init()
{
    if (!_sl_semaphore) {
        _sl_semaphore = xSemaphoreCreateBinary();
    }
}

int sl_enter_critical_region()
{
    return xSemaphoreTake(_sl_semaphore, portMAX_DELAY);
}

int sl_exit_critical_region()
{
    return xSemaphoreGive(_sl_semaphore);
}
