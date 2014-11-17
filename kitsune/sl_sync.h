#ifndef __SL_SYNC_H__
#define __SL_SYNC_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define SL_SYNC(call) \
	({ \
	UARTprintf("enter\n"); \
	long sl_ret; \
	sl_enter_critical_region(); \
	sl_ret = (call); \
	sl_exit_critical_region(); \
	sl_ret; \
	})

#define sl_Start(...) SL_SYNC(sl_Start(__VA_ARGS__))

long sl_sync_init();
long sl_enter_critical_region();
long sl_exit_critical_region();

#ifdef __cplusplus
}
#endif
    
#endif
