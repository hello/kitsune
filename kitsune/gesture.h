/**
 * Gesture control task
 */
#ifndef GESTURE_H
#define GESTURE_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

//user supply callback on detection
//duration and height are not implemented yet
typedef struct{
	void (*on_wave)(int duration, int height);
	void (*on_hold)(int duration, int height);
}gesture_callbacks_t;

void gesture_init(gesture_callbacks_t * user);
void gesture_input(int prox, int light);

#ifdef __cplusplus
}
#endif
    
#endif
