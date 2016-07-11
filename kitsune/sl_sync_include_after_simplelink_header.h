#ifndef __SL_SYNC_H__
#define __SL_SYNC_H__

#include <stdint.h>
#include <stdbool.h>
#include "socket.h"

//#define SL_DEBUG_LOG

#ifdef __cplusplus
extern "C" {
#endif

#include "kit_assert.h"
#include "ustdlib.h"

long sl_sync_init();
long sl_enter_critical_region();
long sl_exit_critical_region();

#ifdef __cplusplus
}
#endif
    
#endif
