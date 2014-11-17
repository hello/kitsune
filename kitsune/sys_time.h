#ifndef __SYSTEM_TIME_H__
#define __SYSTEM_TIME_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

uint64_t get_cache_time();
unsigned long get_time();

#ifdef __cplusplus
}
#endif
    
#endif
