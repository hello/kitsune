

#ifndef __ASSERT_H__
#define __ASSERT_H__

extern void mcu_reset();
#include "uart_logger.h"
#include "FreeRTOS.h"
#include "task.h"

#define assert(x) if(!(x)) {LOGE( "xkd " #x "\n" ); uart_logger_flush(); vTaskDelay(10000); mcu_reset();}

#endif // __USTDLIB_H__
