#ifndef __SL_SYNC_H__
#define __SL_SYNC_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define SL_SYNC(call) \
   ({   \
        sl_enter_critical_region(); \
        int ret = (call); \
        sl_exit_critical_region(); \
        ret; \
   })

int sl_sync_init();
int sl_enter_critical_region();
int sl_exit_critical_region();

#ifdef __cplusplus
}
#endif
    
#endif
