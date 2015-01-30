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

#define WAIT_FOREVER	(0)

bool wait_for_time(int wait_sec);
bool has_good_time();
time_t get_time();

void set_sl_time( time_t unix_timestamp_sec );
time_t get_sl_time( );

uint32_t fetch_unix_time_from_ntp(); //DO NOT CALL THIS ONE EXCEPT IN TESTS
int cmd_set_time(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
    
#endif
