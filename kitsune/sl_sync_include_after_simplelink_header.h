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
extern int sync_ln;
extern const char * sync_fnc;

void _checkret(bool assert_ret, portTickType start);
#ifdef SL_DEBUG_LOG
#define SL_SYNC(call) \
	({ \
	long sl_ret; \
	portTickType start = xTaskGetTickCount();\
	if( sl_enter_critical_region() ){\
		sync_ln = __LINE__;\
		sync_fnc = __FUNCTION__;\
		sl_ret = (call); \
		sl_exit_critical_region(); \
		_checkret( true, start );\
	} else { \
	    _checkret( false, start );\
	}\
	sl_ret; \
	})
#else
#define SL_SYNC(call) \
	({ \
	long sl_ret; \
	assert( sl_enter_critical_region() );\
	sl_ret = (call); \
	sl_exit_critical_region(); \
	sl_ret; \
	})
#endif

#define socket(...)                              sl_Socket(__VA_ARGS__)
#define close(...)                               sl_Close(__VA_ARGS__)
#define accept(...)                              sl_Accept(__VA_ARGS__)
#define bind(...)                                sl_Bind(__VA_ARGS__)
#define listen(...)                              sl_Listen(__VA_ARGS__)
#define connect(...)                             sl_Connect(__VA_ARGS__)
#define select(...)                              sl_Select(__VA_ARGS__)
#define setsockopt(...)                          sl_SetSockOpt(__VA_ARGS__)
#define getsockopt(...)                          sl_GetSockOpt(__VA_ARGS__)
#define recv(...)                                sl_Recv(__VA_ARGS__)
#define recvfrom(...)                            sl_RecvFrom(__VA_ARGS__)
#define write(...)                               sl_Write(__VA_ARGS__)
#define send(...)                                sl_Send(__VA_ARGS__)
#define sendto(...)                              sl_SendTo(__VA_ARGS__)
#define gethostbyname(...)                       sl_NetAppDnsGetHostByName(__VA_ARGS__)
#define htonl(...)                               sl_Htonl(__VA_ARGS__)
#define ntohl(...)                               sl_Ntohl(__VA_ARGS__)
#define htons(...)                               sl_Htons(__VA_ARGS__)
#define ntohs(...)                               sl_Ntohs(__VA_ARGS__)

#if 0

#define sl_Start(...)                            SL_SYNC(sl_Start(__VA_ARGS__))
#define sl_Stop(...)                             SL_SYNC(sl_Stop(__VA_ARGS__))
#define sl_DevSet(...)                           SL_SYNC(sl_DevSet(__VA_ARGS__))
#define sl_DevGet(...)                           SL_SYNC(sl_DevGet(__VA_ARGS__))
#define sl_EventMaskSet(...)                     SL_SYNC(sl_EventMaskSet(__VA_ARGS__))
#define sl_EventMaskGet(...)                     SL_SYNC(sl_EventMaskGet(__VA_ARGS__))
#define sl_Task(...)                             SL_SYNC(sl_Task(__VA_ARGS__))
#define sl_UartSetMode(...)                      SL_SYNC(sl_UartSetMode(__VA_ARGS__))
#define sl_FsOpen(...)                           SL_SYNC(sl_FsOpen(__VA_ARGS__))
#define sl_FsClose(...)                          SL_SYNC(sl_FsClose(__VA_ARGS__))
#define sl_FsRead(...)                           SL_SYNC(sl_FsRead(__VA_ARGS__))
#define sl_FsWrite(...)                          SL_SYNC(sl_FsWrite(__VA_ARGS__))
#define sl_FsGetInfo(...)                        SL_SYNC(sl_FsGetInfo(__VA_ARGS__))
#define sl_FsDel(...)                            SL_SYNC(sl_FsDel(__VA_ARGS__))
#define sl_NetAppStart(...)                      SL_SYNC(sl_NetAppStart(__VA_ARGS__))
#define sl_NetAppStop(...)                       SL_SYNC(sl_NetAppStop(__VA_ARGS__))
#define sl_NetAppDnsGetHostByName(...)           SL_SYNC(sl_NetAppDnsGetHostByName(__VA_ARGS__))
#define sl_NetAppDnsGetHostByService(...)        SL_SYNC(sl_NetAppDnsGetHostByService(__VA_ARGS__))
#define sl_NetAppGetServiceList(...)             SL_SYNC(sl_NetAppGetServiceList(__VA_ARGS__))
#define sl_NetAppMDNSUnRegisterService(...)      SL_SYNC(sl_NetAppMDNSUnRegisterService(__VA_ARGS__))
#define sl_NetAppMDNSRegisterService(...)        SL_SYNC(sl_NetAppMDNSRegisterService(__VA_ARGS__))
#define sl_NetAppPingStart(...)                  SL_SYNC(sl_NetAppPingStart(__VA_ARGS__))
#define sl_NetAppSet(...)                        SL_SYNC(sl_NetAppSet(__VA_ARGS__))
#define sl_NetAppGet(...)                        SL_SYNC(sl_NetAppGet(__VA_ARGS__))
#define sl_NetCfgSet(...)                        SL_SYNC(sl_NetCfgSet(__VA_ARGS__))
#define sl_NetCfgGet(...)                        SL_SYNC(sl_NetCfgGet(__VA_ARGS__))
#define sl_Socket(...)                           SL_SYNC(sl_Socket(__VA_ARGS__))
#define sl_Close(...)                            SL_SYNC(sl_Close(__VA_ARGS__))
#define sl_Accept(...)                           SL_SYNC(sl_Accept(__VA_ARGS__))
#define sl_Bind(...)                             SL_SYNC(sl_Bind(__VA_ARGS__))
#define sl_Listen(...)                           SL_SYNC(sl_Listen(__VA_ARGS__))
#define sl_Connect(...)                          SL_SYNC(sl_Connect(__VA_ARGS__))
#define sl_Select(...)                           SL_SYNC(sl_Select(__VA_ARGS__))
#define sl_SetSockOpt(...)                       SL_SYNC(sl_SetSockOpt(__VA_ARGS__))
#define sl_GetSockOpt(...)                       SL_SYNC(sl_GetSockOpt(__VA_ARGS__))
#define sl_Recv(...)                             SL_SYNC(sl_Recv(__VA_ARGS__))
#define sl_RecvFrom(...)                         SL_SYNC(sl_RecvFrom(__VA_ARGS__))
#define sl_Send(...)                             SL_SYNC(sl_Send(__VA_ARGS__))
#define sl_SendTo(...)                           SL_SYNC(sl_SendTo(__VA_ARGS__))
#define sl_Htonl(...)                            SL_SYNC(sl_Htonl(__VA_ARGS__))
#define sl_Htons(...)                            SL_SYNC(sl_Htons(__VA_ARGS__))
#define sl_WlanConnect(...)                      SL_SYNC(sl_WlanConnect(__VA_ARGS__))
#define sl_WlanDisconnect(...)                   SL_SYNC(sl_WlanDisconnect(__VA_ARGS__))
#define sl_WlanProfileAdd(...)                   SL_SYNC(sl_WlanProfileAdd(__VA_ARGS__))
#define sl_WlanProfileGet(...)                   SL_SYNC(sl_WlanProfileGet(__VA_ARGS__))
#define sl_WlanProfileDel(...)                   SL_SYNC(sl_WlanProfileDel(__VA_ARGS__))
#define sl_WlanPolicySet(...)                    SL_SYNC(sl_WlanPolicySet(__VA_ARGS__))
#define sl_WlanPolicyGet(...)                    SL_SYNC(sl_WlanPolicyGet(__VA_ARGS__))
#define sl_WlanGetNetworkList(...)               SL_SYNC(sl_WlanGetNetworkList(__VA_ARGS__))
#define sl_WlanRxStatStart(...)                  SL_SYNC(sl_WlanRxStatStart(__VA_ARGS__))
#define sl_WlanRxStatStop(...)                   SL_SYNC(sl_WlanRxStatStop(__VA_ARGS__))
#define sl_WlanRxStatGet(...)                    SL_SYNC(sl_WlanRxStatGet(__VA_ARGS__))
#define sl_WlanSmartConfigStop(...)              SL_SYNC(sl_WlanSmartConfigStop(__VA_ARGS__))
#define sl_WlanSmartConfigStart(...)             SL_SYNC(sl_WlanSmartConfigStart(__VA_ARGS__))
#define sl_WlanSetMode(...)                      SL_SYNC(sl_WlanSetMode(__VA_ARGS__))
#define sl_WlanSet(...)                          SL_SYNC(sl_WlanSet(__VA_ARGS__))
#define sl_WlanGet(...)                          SL_SYNC(sl_WlanGet(__VA_ARGS__))
#define sl_WlanRxFilterAdd(...)                  SL_SYNC(sl_WlanRxFilterAdd(__VA_ARGS__))
#define sl_WlanRxFilterSet(...)                  SL_SYNC(sl_WlanRxFilterSet(__VA_ARGS__))
#define sl_WlanRxFilterGet(...)                  SL_SYNC(sl_WlanRxFilterGet(__VA_ARGS__))
#endif
long sl_sync_init();
long sl_enter_critical_region();
long sl_exit_critical_region();

#ifdef __cplusplus
}
#endif
    
#endif
