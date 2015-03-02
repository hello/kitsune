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
static struct{
	led_color_t colors[NUM_LED];
	int progress_bar_percent;
	uint8_t trippy_base[3];
	uint8_t trippy_range[3];
}self;

typedef struct{
	led_color_t color;
	int repeat;
}wheel_context;

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
static bool _animate_solid(const led_color_t * prev, led_color_t * out, void * user_context, unsigned int counter){
	int i;
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

		for(i = 0; i < NUM_LED; i++){
			out[i] = color;
		}
		ctx->ctr += 6;
		//UARTprintf("roll %d\n", ctx->ctr);
	}
	return true;
}
static bool _animate_trippy(const led_color_t * prev, led_color_t * out, void * user_context, unsigned int counter){
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
static bool _animate_progress(const led_color_t * prev, led_color_t * out, void * user_context, unsigned int counter){
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
static bool _animate_factory_test_pattern(const led_color_t * prev, led_color_t * out, void * user_context, unsigned int counter){
	int i;
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

static bool _animate_wheel(const led_color_t * prev, led_color_t * out, void * user_context, unsigned int counter){
	bool ret = true;
	if(user_context){
		int i;
		unsigned int ctr;
		wheel_context * ctx = user_context;
		ctr = counter * 6;
		for(i = 0; i < NUM_LED; i++){
			out[i] = wheel_color(((i * 256 / 12) - ctr) & 255, ctx->color);
			if(ctr < 128){
				out[i] = led_from_brightness(&out[i], ctr * 2);
			}
			if(ctx->repeat){
				int fade = 255;
				if(ctr > ((ctx->repeat - 1) * 256)){
					fade =  (ctr >= ctx->repeat * 256)?0:(256 - ctr % 256);
				}
				if(fade == 0){
					ret = false;
				}
				out[i] = led_from_brightness(&out[i],fade);
			}
		}
	}
	return ret;
}

/*
 * Pubic, call these to set up animationz
 */

void init_led_animation() {
	//self._sem = xSemaphoreCreateRecursiveMutex();
}

int play_led_trippy(uint8_t trippy_base[3], uint8_t trippy_range[3], unsigned int timeout){
	int i, ret;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_trippy,
		.context = NULL,
		.priority = 1,
		.initial_state = {0},
		.cycle_time = 30,
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
int play_led_animation_stop(void){
	int ret;
	user_animation_t anim = (user_animation_t){
				.handler = NULL,
				.context = NULL,
				.priority = 0,
				.initial_state = {0},
				.cycle_time = 33,
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
		user_animation_t anim = (user_animation_t){
			.handler = _animate_factory_test_pattern,
			.context = NULL,
			.priority = 2,
			.initial_state = {0},
			.cycle_time = 500,
		};
		ret = led_transition_custom_animation(&anim);
		return ret;
}
int play_led_wheel(int a, int r, int g, int b, int repeat, int delay){
	int ret;
	static wheel_context ctx;
	led_color_t color = led_from_rgb(r,g,b);
	color = led_from_brightness( &color, a );

	ctx.color =  color;
	ctx.repeat = repeat;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_wheel,
		.context = &ctx,
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

void stop_led_animation(unsigned int delay){
	ANIMATE_BLOCKING(play_led_animation_stop(),delay);
}

int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			play_led_animation_stop();
		}else if(strcmp(argv[1], "+") == 0){
			set_led_progress_bar(self.progress_bar_percent += 5);
		}else if(strcmp(argv[1], "-") == 0){
			set_led_progress_bar(self.progress_bar_percent -= 5);
		}else if(strcmp(argv[1], "trippy") == 0){
			if(argc >= 8){
				uint8_t trippy_base[3] = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4])};
				uint8_t trippy_range[3] = {atoi(argv[5]), atoi(argv[6]), atoi(argv[7])};
				play_led_trippy( trippy_base, trippy_range, portMAX_DELAY );
			}else{
				uint8_t trippy_base[3] = {rand()%120, rand()%120, rand()%120};
				uint8_t trippy_range[3] = {rand()%120, rand()%120, rand()%120};
				play_led_trippy( trippy_base, trippy_range, portMAX_DELAY );
			}
		}else if(strcmp(argv[1], "wheel") == 0){
			play_led_wheel(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 2, 16);
		}else if(strcmp(argv[1], "solid") == 0){
			play_led_animation_solid(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 1,18);
		}else if(strcmp(argv[1], "prog") == 0){
			play_led_progress_bar(20, 20, 20, 0, portMAX_DELAY);
		}else if(strcmp(argv[1], "kill") == 0){
			stop_led_animation(1);
		}
		return 0;
	}else{
		return -1;
	}
}

