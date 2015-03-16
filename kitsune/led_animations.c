#include <hw_memmap.h>
#include <stdlib.h>
#include "rom_map.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "bigint_impl.h"
#include "uart_logger.h"

#include "led_animations.h"
typedef struct{
	led_color_t color;
	int repeat;
	int ctr;
	int fade;
}wheel_context;

static struct{
	led_color_t colors[NUM_LED];
	int progress_bar_percent;
	uint8_t trippy_base[3];
	uint8_t trippy_range[3];
	wheel_context wheel_ctx;
}self;


extern xSemaphoreHandle led_smphr;

static bool _reach_color(unsigned int * v, unsigned int target, int step_size){
	if(*v == target){
		return true;
	}else if(*v < target){
		*v = *v + min(step_size, (target-*v));
	}else{
		*v = *v - min(step_size, (*v - target));
	}
	return false;
}
static int _new_random_color(uint8_t range, uint8_t base){
	return ((unsigned int)rand()) % range + base;
}

//#include "uartstdio.h"
typedef struct {
	led_color_t color;
	int alpha;
	int repeat;
	int ctr;
} _animate_solid_ctx;
static bool _animate_solid(const led_color_t * prev, led_color_t * out, void * user_context){
	out->rgb = 0;
	if(user_context){
		_animate_solid_ctx * ctx = ((_animate_solid_ctx *)user_context);
		led_color_t color = led_from_brightness(&ctx->color, ctx->alpha);
		if(ctx->ctr < 254) {
			//UARTprintf("fi %d\n", ctx->ctr);
			color = led_from_brightness(&ctx->color, ctx->ctr);
		} else if(ctx->ctr < 508 ) {
			//UARTprintf("fo %d\n", ctx->ctr);
			color = led_from_brightness(&ctx->color, 508-ctx->ctr);
		} else {
			color.rgb = 0;
			//UARTprintf("ovr %d %d\n", ctx->ctr, ctx->repeat);
			ctx->ctr = 0;
			if (--ctx->repeat <= 0) {
				return false;
			}
		}

		ledset(out, color, NUM_LED);
		ctx->ctr += 6;
		//UARTprintf("roll %d\n", ctx->ctr);
	}
	return true;
}
static bool _animate_trippy(const led_color_t * prev, led_color_t * out, void * user_context){
	int i = 0;
	static int scaler = 100;
	for(i = 0; i < NUM_LED; i++){
		unsigned int r0,r1,g0,g1,b0,b1;
		led_to_rgb(&prev[i], &r0, &g0, &b0);
		led_to_rgb(&self.colors[i],&r1, &g1, &b1);
		if(_reach_color(&r0, r1, 1)){
			r1 = _new_random_color(self.trippy_range[0], self.trippy_base[0]);
		}
		if(_reach_color(&g0, g1, 1)){
			g1 = _new_random_color(self.trippy_range[1], self.trippy_base[1]);
		}
		if(_reach_color(&b0, b1, 1)){
			b1 = _new_random_color(self.trippy_range[2], self.trippy_base[2]);
		}
		self.colors[i] = led_from_rgb(r1,g1,b1);
		out[i] = led_from_rgb(
				r0 * ((unsigned int)(scaler)) / 100,
				g0 * ((unsigned int)(scaler)) / 100,
				b0 * ((unsigned int)(scaler)) / 100);
	}
	return true;
}
static bool _animate_progress(const led_color_t * prev, led_color_t * out, void * user_context){
	int prog = self.progress_bar_percent * NUM_LED;
	int filled = prog / 100;
	int left = ((prog % 100)*254)>>8;

	int i;
	for(i = 0; i < filled && i < NUM_LED; i++){
		out[i] = self.colors[0];
	}
	if(filled < NUM_LED ){
		unsigned int r,g,b;
		led_to_rgb(&self.colors[0], &r, &g, &b);
		out[filled] = led_from_rgb(
				 ((r * left)>>8)&0xff,
				 ((g * left)>>8)&0xff,
				 ((b * left)>>8)&0xff);
	}
	return true;
}
static bool _animate_factory_test_pattern(const led_color_t * prev, led_color_t * out, void * user_context ){
	int i;
	int counter = *(int*)user_context;
	for(i = 0; i < NUM_LED; i++){
		if( ( (i+counter) % 3 ) == 0 ) {
			out[i] = led_from_rgb(255,255,255);
		} else {
			out[i] = led_from_rgb(0,0,0);
		}
	}
	return true;
}
static led_color_t wheel_color(int WheelPos, led_color_t color) {
	unsigned int r, g, b;
	led_to_rgb(&color, &r, &g, &b);

	if (WheelPos < 85) {
		return led_from_rgb(0, 0, 0);
	} else if (WheelPos < 170) {
		WheelPos -= 85;
		return led_from_rgb((r * (WheelPos * 3)) >> 8,
				(g * (WheelPos * 3)) >> 8, (b * (WheelPos * 3)) >> 8);
	} else {
		WheelPos -= 170;
		return led_from_rgb((r * (255 - WheelPos * 3)) >> 8,
				(g * (255 - WheelPos * 3)) >> 8,
				(b * (255 - WheelPos * 3)) >> 8);
	}
}

static bool _animate_wheel(const led_color_t * prev, led_color_t * out, void * user_context ){
	if(user_context){
		int i;
		wheel_context * ctx = user_context;

		xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
		ctx->ctr += 6;
		for(i = 0; i < NUM_LED; i++){
			out[i] = wheel_color(((i * 256 / 12) - ctx->ctr) & 255, ctx->color);
			if(ctx->ctr < 128){
				out[i] = led_from_brightness(&out[i], ctx->ctr * 2);
			}
		}
		if(ctx->fade){
			return false;
		}
		xSemaphoreGiveRecursive(led_smphr);
	}
	return true;
}

/*
 * Pubic, call these to set up animationz
 */

void init_led_animation() {
	//self._sem = xSemaphoreCreateRecursiveMutex();
}

int play_led_trippy(uint8_t trippy_base[3], uint8_t trippy_range[3], unsigned int timeout, unsigned int delay ){
	int i, ret;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_trippy,
		.context = NULL,
		.priority = 1,
		.initial_state = {0},
		.cycle_time = delay,
	};

	for(i = 0; i < NUM_LED; i++){
		anim.initial_state[i] = led_from_rgb(
				_new_random_color(trippy_range[0],trippy_base[0]),
				_new_random_color(trippy_range[1],trippy_base[1]),
				_new_random_color(trippy_range[2],trippy_base[2]));
	}
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	ret = led_transition_custom_animation(&anim);
	if( ret > 0 ) {
		memcpy(self.trippy_base, trippy_base, 3);
		memcpy(self.trippy_range, trippy_range, 3);

		for(i = 0; i < NUM_LED; i++){
			self.colors[i] = anim.initial_state[i];
		}
	}
	xSemaphoreGiveRecursive(led_smphr);
	return ret;

}
int play_led_animation_stop(unsigned int fadeout){
	int ret;
	user_animation_t anim = (user_animation_t){
				.handler = NULL,
				.context = NULL,
				.priority = 0,
				.initial_state = {0},
				.cycle_time = fadeout,
	};
	ret = led_transition_custom_animation(&anim);
	return ret;
}
int play_led_animation_solid(int a, int r, int g, int b, int repeat, int delay){
	static _animate_solid_ctx ctx;
	int ret;

	ctx.color = led_from_rgb(r, g, b);
	ctx.alpha = a;
	ctx.ctr = 0;
	ctx.repeat = repeat;
	user_animation_t anim = (user_animation_t){
			.handler = _animate_solid,
			.context = &ctx,
			.priority = 1,
			.initial_state = {0},
			.cycle_time = delay,
	};
	ret = led_transition_custom_animation(&anim);
	return ret;
}
int play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout){
	int ret;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_progress,
		.context = NULL,
		.priority = 2,
		.initial_state = {0},
		.cycle_time = 20,
	};
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	ret = led_transition_custom_animation(&anim);
	if( ret > 0 ) {
		self.colors[0] = led_from_rgb(r, g, b);
		self.progress_bar_percent = 0;
	}
	xSemaphoreGiveRecursive(led_smphr);
	return ret;
}
int factory_led_test_pattern(unsigned int timeout) {
	int ret;
	static int counter;
	counter = 0;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_factory_test_pattern,
		.context = (void*)&counter,
		.priority = 2,
		.initial_state = {0},
		.cycle_time = 500,
	};
	ret = led_transition_custom_animation(&anim);
	return ret;
}

int stop_led_wheel() {
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	self.wheel_ctx.repeat = 1;
	self.wheel_ctx.fade = 1;
	xSemaphoreGiveRecursive(led_smphr);
	return 0;
}
int play_led_wheel(int a, int r, int g, int b, int repeat, int delay){
	int ret;
	led_color_t color = led_from_rgb(r,g,b);
	color = led_from_brightness( &color, a );

	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	self.wheel_ctx.color =  color;
	self.wheel_ctx.ctr = 0;
	self.wheel_ctx.repeat = repeat;
	self.wheel_ctx.fade = 0;
	xSemaphoreGiveRecursive(led_smphr);

	user_animation_t anim = (user_animation_t){
		.handler = _animate_wheel,
		.context = &self.wheel_ctx,
		.priority = 2,
		.initial_state = {0},
		.cycle_time = delay,
	};
	ret = led_transition_custom_animation(&anim);
	return ret;
}
void set_led_progress_bar(uint8_t percent){
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	self.progress_bar_percent =  percent > 100?100:percent;
	xSemaphoreGiveRecursive(led_smphr);
}

void stop_led_animation(unsigned int delay, unsigned int fadeout){
//	ANIMATE_BLOCKING(play_led_animation_stop(fadeout),delay);
	led_fade_out_custom_animation();
}

int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			play_led_animation_stop(33);
		}else if(strcmp(argv[1], "+") == 0){
			set_led_progress_bar(self.progress_bar_percent += 5);
		}else if(strcmp(argv[1], "-") == 0){
			set_led_progress_bar(self.progress_bar_percent -= 5);
		}else if(strcmp(argv[1], "trippy") == 0){
			if(argc >= 8){
				uint8_t trippy_base[3] = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4])};
				uint8_t trippy_range[3] = {atoi(argv[5]), atoi(argv[6]), atoi(argv[7])};
				play_led_trippy( trippy_base, trippy_range, portMAX_DELAY, 30 );
			}else{
				uint8_t trippy_base[3] = {rand()%120, rand()%120, rand()%120};
				uint8_t trippy_range[3] = {rand()%120, rand()%120, rand()%120};
				play_led_trippy( trippy_base, trippy_range, portMAX_DELAY, 30 );
			}
		}else if(strcmp(argv[1], "wheel") == 0){
			play_led_wheel(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 2, 16);
		}else if(strcmp(argv[1], "wheelr") == 0){
			play_led_wheel(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 0, 16);
		}else if(strcmp(argv[1], "wheels") == 0){
			stop_led_wheel();
		}else if(strcmp(argv[1], "solid") == 0){
			play_led_animation_solid(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 1,18);
		}else if(strcmp(argv[1], "prog") == 0){
			play_led_progress_bar(20, 20, 20, 0, portMAX_DELAY);
		}else if(strcmp(argv[1], "kill") == 0){
			stop_led_animation(1,1);
		}
		return 0;
	}else{
		return -1;
	}
}

