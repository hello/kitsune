#ifndef __SYSTEM_TIME_H__
#define __SYSTEM_TIME_H__

#include <stdint.h>
#include <stdbool.h>

#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_SYS_TIME	(0xFFFFFFFF)

void init_time_module( int stack );

void wait_for_time();
bool has_good_time();
time_t get_time();

uint32_t fetch_unix_time_from_ntp(); //DO NOT CALL THIS ONE EXCEPT IN TESTS


#ifdef __cplusplus
}
#endif
    
#endif
