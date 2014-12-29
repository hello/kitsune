/**
 * Gesture control task
 */
#ifndef GESTURE_H
#define GESTURE_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	GESTURE_NONE = 0,
	GESTURE_WAVE,
	GESTURE_HOLD,
	GESTURE_OUT,
} gesture;

void gesture_init();
gesture gesture_input(int prox);
int gesture_get_wave_count();
int gesture_get_hold_count();
void gesture_counter_reset();

#ifdef __cplusplus
}
#endif
    
#endif
