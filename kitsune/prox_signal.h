#ifndef _PROXSIGNAL_H_
#define _PROXSIGNAL_H_

#include <stdint.h>

#define MAX_HOLD_TIME_MS 15000

typedef enum {
	proxGestureNone,
	proxGestureWave,
	proxGestureHold,
	proxGestureRelease
} ProxGesture_t;

void ProxSignal_Init(void);

int32_t ProxSignal_MedianFilter(const int32_t x) ;

ProxGesture_t ProxSignal_UpdateChangeSignals(const int32_t newx) ;


#endif //_PROXSIGNAL_H_
