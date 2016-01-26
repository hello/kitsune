

#ifndef __ASSERT_H__
#define __ASSERT_H__

extern void mcu_reset();
#include "uart_logger.h"
#include "FreeRTOS.h"
#include "task.h"

void
vAssertCalled( const char * s );

// #x here is too much text
#define assert(x) if(!(x)) {vAssertCalled("yolo");}

#endif // __USTDLIB_H__
