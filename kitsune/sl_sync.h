#ifndef __SL_SYNC_H__
#define __SL_SYNC_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define SL_SYNC(call) \
	({ \
	long sl_ret; \
	sl_enter_critical_region(); \
	sl_ret = (call); \
	sl_exit_critical_region(); \
	sl_ret; \
	})

long sl_sync_init();
long sl_enter_critical_region();
long sl_exit_critical_region();

#ifdef __cplusplus
}
#endif
    
#endif
