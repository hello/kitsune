

#ifndef __ASSERT_H__
#define __ASSERT_H__

extern void mcu_reset();
#include "uart_logger.h"
#include "FreeRTOS.h"
#include "task.h"

void
vAssertCalled( const char * s );

#define assert(x) if(!(x)) {vAssertCalled(#x);}

//#define DEMO

#endif // __USTDLIB_H__
