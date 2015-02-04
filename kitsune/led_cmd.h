#ifndef LED_CMD_H
#define LED_CMD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define NUM_LED 12
#define LED_MAX 254
int Cmd_led_clr(int argc, char *argv[]);
int Cmd_led(int argc, char *argv[]);
int led_init(void);

void led_idle_task( void * params );
void led_task( void * params );

typedef bool (*led_user_animation_handler)(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size);
//helper api
int led_set_color(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b, int fade_in, int fade_out, unsigned int ud, int rot);
int led_transition_custom_animation(led_user_animation_handler user, void * context, const uint32_t * initial_colors);
int led_start_custom_animation(led_user_animation_handler user, void * user_context);
void led_get_user_color(uint8_t* out_red, uint8_t* out_green, uint8_t* out_blue);
void led_set_user_color(uint8_t red, uint8_t green, uint8_t blue);
int led_set_color(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b,
		int fade_in, int fade_out,
		unsigned int ud,
		int rot);
int led_ready();
bool led_wait_for_idle(unsigned int wait);
void led_fadeout(unsigned int dly);
unsigned int led_delay( unsigned int dly );

#ifdef __cplusplus
}
#endif
#endif
