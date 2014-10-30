#ifndef LED_CMD_H
#define LED_CMD_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

int Cmd_led_clr(int argc, char *argv[]);
int Cmd_led(int argc, char *argv[]);
int led_init(void);
void led_task( void * params );

typedef bool (*led_user_animation_handler)(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size);
//helper api
int led_set_color(int r, int g, int b, int fade);
int led_start_custom_animation(led_user_animation_handler user, void * user_context);
#ifdef __cplusplus
}
#endif
#endif
