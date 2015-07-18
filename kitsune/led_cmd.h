#ifndef LED_CMD_H
#define LED_CMD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define NUM_LED 12
#define LED_MAX 254
#define PROGRESS_COMPLETE LED_MAX

typedef struct{
	unsigned int rgb;
}led_color_t;

void led_to_rgb(const led_color_t * c, unsigned int *r, unsigned int* g, unsigned int* b);
led_color_t led_from_rgb( int r, int g, int b);
led_color_t led_from_brightness(const led_color_t * c, unsigned int br);
void ledset(led_color_t * dst, led_color_t src, int num);
void ledcpy(led_color_t * dst, const led_color_t * src, int num);

enum {
	ANIMATION_STOP,
	ANIMATION_CONTINUE,
	ANIMATION_FADEOUT,
};

typedef int (*led_user_animation_handler)(const led_color_t * prev, led_color_t * out, void * user_context );
typedef bool (*led_user_animation_reinit_handler)( void * user_context );
typedef struct{
	led_user_animation_handler handler;
	led_user_animation_reinit_handler reinit_handler;
	void * context;
	uint8_t priority;
	int fade_time;
	int cycle_time;
	int fade_elapsed;
	uint32_t opt; //bitfield
#define TRANSITION_WITHOUT_FADE 0x00000001

}user_animation_t;

int Cmd_led_clr(int argc, char *argv[]);
int Cmd_led(int argc, char *argv[]);
int led_init(void);

void led_idle_task( void * params );
void led_task( void * params );

//helper api

int led_set_color(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b, int fade_in, int fade_out, unsigned int ud, int rot);
/**
 * returns -1 if error, else animation id
 */
int led_transition_custom_animation(const user_animation_t * user);
int led_fade_current_animation(void);
int led_fade_all_animation(int fadeout);
int led_get_animation_id(void);
void flush_animation_history();

void led_get_user_color(unsigned int* out_red, unsigned int* out_green, unsigned int* out_blue, bool light);
void led_set_user_color(unsigned int red, unsigned int green, unsigned int blue, bool light);
int led_set_color(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b,
		int fade_in, int fade_out,
		unsigned int ud,
		int rot);
bool led_is_idle(unsigned int wait);
unsigned int led_delay( unsigned int dly );

#ifdef __cplusplus
}
#endif
#endif
