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
int gesture_get_wave_count();
int gesture_get_hold_count();
void gesture_counter_reset();
void gesture_increment_wave_count();
void gesture_increment_hold_count();
int gesture_get_and_reset_all_diagnostic_counts();

#ifdef __cplusplus
}
#endif
    
#endif
