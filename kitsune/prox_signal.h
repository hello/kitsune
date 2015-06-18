#ifndef _PROXSIGNAL_H_
#define _PROXSIGNAL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
	proxGestureNone,
	proxGestureWave,
	proxGestureHold,
	proxGestureRelease
} ProxGesture_t;

void ProxSignal_Init(void);

ProxGesture_t ProxSignal_UpdateChangeSignals(const int32_t newx) ;

#ifdef __cplusplus
}
#endif

#endif //_PROXSIGNAL_H_
