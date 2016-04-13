#ifndef _PROXSIGNAL_H_
#define _PROXSIGNAL_H_

#include <stdint.h>

#define MAX_HOLD_TIME_MS 15000

typedef enum {
	proxGestureNone,
	proxGestureWave,
	proxGestureHold,
	proxGestureHeld,
	proxGestureRelease,
	proxGestureIncoming
} ProxGesture_t;

void ProxSignal_Init(void);

ProxGesture_t ProxSignal_UpdateChangeSignals( int32_t newx) ;


#endif //_PROXSIGNAL_H_
