#ifndef LED_ANIMATIONS_H
#define LED_ANIMATIONS_H

#include "stdbool.h"
#include "led_cmd.h"

#define TRIPPY_FADE_DELAY	(15)

#ifdef __cplusplus
extern "C" {
#endif

void init_led_animation();

//call to stop all animations
void stop_led_animation();
void stop_led_animation_sync(int dly);
int Cmd_led_animate(int argc, char *argv[]);

//custom animations
bool play_led_animation_pulse(unsigned int timeout);
bool play_led_trippy(uint8_t trippy_base[3], uint8_t range[3], unsigned int timeout);
bool play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout);
void set_led_progress_bar(uint8_t percent);
bool factory_led_test_pattern(unsigned int timeout);

#ifdef __cplusplus
}
#endif
#endif
