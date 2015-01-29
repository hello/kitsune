
#include "FreeRTOS.h"
#include "semphr.h"

#include "sl_sync_include_after_simplelink_header.h"

static xSemaphoreHandle _sl_mutex;

long sl_sync_init()
{
	_sl_mutex = xSemaphoreCreateMutex();
	return 1;
}

long sl_enter_critical_region()
{
    return xSemaphoreTake(_sl_mutex, portMAX_DELAY);
}

long sl_exit_critical_region()
{
    return xSemaphoreGive(_sl_mutex);
}
long sl_gethostbynameNoneThreadSafe(_i8 * hostname,const  _u16 usNameLen, _u32*  out_ip_addr,const _u8 family ) {
#ifdef sl_NetAppDnsGetHostByName
#undef sl_NetAppDnsGetHostByName
#endif
	sl_enter_critical_region();
	sl_exit_critical_region();
	return sl_NetAppDnsGetHostByName(hostname, usNameLen, out_ip_addr, family);
#define sl_NetAppDnsGetHostByName(...) SL_SYNC(sl_NetAppDnsGetHostByName(__VA_ARGS__))
}

_i16 sl_recvfromNoneThreadSafe(_i16 sd, void *buf, _i16 Len, _i16 flags, SlSockAddr_t *from, SlSocklen_t *fromlen){
#ifdef sl_RecvFrom
#undef sl_RecvFrom
#endif
	sl_enter_critical_region();
	sl_exit_critical_region();
	return sl_RecvFrom(sd, buf, Len, flags, from, fromlen);
#define sl_RecvFrom(...) SL_SYNC(sl_RecvFrom(__VA_ARGS__))
}

long sl_AcceptNoneThreadSafe(_i16 sd, SlSockAddr_t *addr, SlSocklen_t *addrlen)
{
#ifdef sl_Accept
#undef sl_Accept
#endif
	sl_enter_critical_region();
	sl_exit_critical_region();
	return sl_Accept(sd, addr, addrlen);
#define sl_Accept(...) SL_SYNC(sl_Accept(__VA_ARGS__))
}
