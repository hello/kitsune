#ifndef __SYSTEM_TIME_H__
#define __SYSTEM_TIME_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_SYS_TIME	(0xFFFFFFFF)

static int _init = 0;

inline int time_module_initialized()
{
	return _init == 1;
}

inline int init_time_module()
{
	_init = 1;
	return _init == 1;
}

uint32_t set_nwp_time(uint32_t unix_timestamp_sec);
uint32_t get_nwp_time();
uint32_t fetch_time_from_ntp_server();

#ifdef __cplusplus
}
#endif
    
#endif
