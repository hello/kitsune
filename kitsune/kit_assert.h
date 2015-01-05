

#ifndef __ASSERT_H__
#define __ASSERT_H__

extern void mcu_reset();
extern void UARTprintf(const char *pcString, ...);
#include "FreeRTOS.h"
#include "task.h"
#define assert(x) if(!(x)) {UARTprintf("! %s %d\n", __FILE__, __LINE__); vTaskDelay(10000); mcu_reset();}

#endif // __USTDLIB_H__
