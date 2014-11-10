/**
 * Gesture control task
 */
#ifndef GESTURE_H
#define GESTURE_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
typedef struct{
	void (*on_wave)(uint8_t wave_depth);
}gesture_callbacks_t;

void gesture_init(gesture_callbacks_t * user);
void gesture_input(int prox, int light);

#ifdef __cplusplus
}
#endif
    
#endif
