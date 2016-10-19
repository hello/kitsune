#ifndef LED_ANIMATIONS_H
#define LED_ANIMATIONS_H

#include "stdbool.h"
#include "led_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_led_animation();

//call to stop *all* animation
void stop_led_animation(unsigned int delay, unsigned int fadeout);
int Cmd_led_animate(int argc, char *argv[]);

//custom animations, stick them in the ANIMATE_BLOCKING macro for blocking version
int play_led_trippy(uint8_t trippy_base[3], uint8_t trippy_range[3], unsigned int timeout, unsigned int delay, unsigned int fade );
int play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout);
int play_led_animation_solid(int a, int r, int g, int b, int repeat, int delay, int priority);
int factory_led_test_pattern(unsigned int timeout);
int play_led_wheel(int a, int r, int g, int b, int repeat, int delay, int priority);
int play_modulation(int r, int g, int b, int delay, int priority);
int play_pairing_glow( void );

void set_led_progress_bar(uint8_t percent);
void set_modulation_intensity(uint8_t intensity);
/*
 * this macro invokes one of the play_led_* animation
 * and blocks until either a higher animation overwrites it, or the animation ends by itself.
 */
#define ANIMATE_BLOCKING(anim, timeout) do{\
	int ret = anim;\
	int to = timeout;\
	while(ret >= 0 && to){\
		if(led_is_idle(0) || ret != led_get_animation_id()){\
			break;\
		}else{\
			to-=10;\
			vTaskDelay(10);\
		}\
	}\
}while(0)
#ifdef __cplusplus
}
#endif
#endif
