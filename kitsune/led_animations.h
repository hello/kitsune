#ifndef LED_ANIMATIONS_H
#define LED_ANIMATIONS_H

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_led_animation();
bool lock_animation();
void unlock_animation();

//call to stop all animations
void stop_led_animation(void);
int Cmd_led_animate(int argc, char *argv[]);

//custom animations
bool play_led_animation_pulse(unsigned int timeout);
bool play_led_trippy(unsigned int timeout);
bool play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout);
void set_led_progress_bar(uint8_t percent);
bool factory_led_test_pattern(unsigned int timeout);

#ifdef __cplusplus
}
#endif
#endif
