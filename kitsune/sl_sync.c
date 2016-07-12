
#include "FreeRTOS.h"
#include "semphr.h"
#include "uart_logger.h"
#include "stdbool.h"

#include "sl_sync_include_after_simplelink_header.h"

long sl_sync_init()
{
	return 1;
}

long sl_enter_critical_region()
{
    	return 1;
}

long sl_exit_critical_region()
{
    return 1;
}
