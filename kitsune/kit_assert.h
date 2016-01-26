

#ifndef __ASSERT_H__
#define __ASSERT_H__

extern void mcu_reset();
#include "uart_logger.h"
#include "FreeRTOS.h"
#include "task.h"

void
vAssertCalled( const char * s );
#define assert(x) if(!(x)) {vAssertCalled("yolo");} // #x here is too much text

#endif // __USTDLIB_H__
