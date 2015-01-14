
#include "FreeRTOS.h"
#include "semphr.h"

#include "sl_sync_include_after_simplelink_header.h"

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

long sl_AcceptNoneThreadSafe(_i16 sd, SlSockAddr_t *addr, SlSocklen_t *addrlen)
{
#ifdef sl_Accept
#undef sl_Accept
#endif
	return sl_Accept(sd, addr, addrlen);
#define sl_Accept(...) SL_SYNC(sl_Accept(__VA_ARGS__))
}
