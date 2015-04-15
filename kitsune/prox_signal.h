#ifndef _PROXSIGNAL_H_
#define _PROXSIGNAL_H_

#include <stdint.h>

typedef enum {
	proxGestureNone,
	proxGestureWave,
	proxGestureHold
} ProxGesture_t;

void ProxSignal_Init(void);

int32_t ProxSignal_MedianFilter(const int32_t x) ;

ProxGesture_t ProxSignal_UpdateChangeSignals(const int32_t newx) ;


#endif //_PROXSIGNAL_H_
