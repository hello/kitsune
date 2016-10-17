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

#include "kit_assert.h"

#include "led_animations.h"
typedef struct{
	led_color_t color;
	int repeat;
	int ctr;
}wheel_context;

static struct{
	led_color_t colors[NUM_LED];
	int progress_bar_percent;
	uint8_t trippy_base[3];
	uint8_t trippy_range[3];
	uint8_t alpha_override;
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
static bool _reinit_animate_solid( void * user_context){
	if(user_context){
		_animate_solid_ctx * ctx = (_animate_solid_ctx *) user_context;
		ctx->ctr = 0;
		return true;
	}
	return false;
}
static int _animate_solid(const led_color_t * prev, led_color_t * out, void * user_context){
	out->rgb = 0;
	if(user_context){
		_animate_solid_ctx * ctx = (_animate_solid_ctx *) user_context;
		led_color_t color = led_from_brightness(&ctx->color, ctx->alpha);
		if(ctx->ctr < 254) {
			//UARTprintf("fi %d\n", ctx->ctr);
			color = led_from_brightness(&color, ctx->ctr);
		} else if(ctx->ctr < 508 ) {
			//UARTprintf("fo %d\n", ctx->ctr);
			color = led_from_brightness(&color, 508-ctx->ctr);
		} else {
			color.rgb = 0;
			//UARTprintf("ovr %d %d\n", ctx->ctr, ctx->repeat);
			ctx->ctr = 0;
			if (--ctx->repeat <= 0) {
				return ANIMATION_STOP;
			}
		}
		ledset(out, color, NUM_LED);
		ctx->ctr += 6;
		//UARTprintf("roll %d\n", ctx->ctr);
	}
	return ANIMATION_CONTINUE;
}
static int _animate_modulation(const led_color_t * prev, led_color_t * out, void * user_context){
	out->rgb = 0;
	if(user_context){
		_animate_solid_ctx * ctx = (_animate_solid_ctx *) user_context;
		led_color_t color = led_from_brightness(&ctx->color, self.alpha_override);
		ledset(out, color, NUM_LED);
		return ANIMATION_CONTINUE;
	}
	return ANIMATION_STOP;
}
static int _animate_trippy(const led_color_t * prev, led_color_t * out, void * user_context){
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
	return ANIMATION_CONTINUE;
}
static int _animate_progress(const led_color_t * prev, led_color_t * out, void * user_context){
	int prog = self.progress_bar_percent * NUM_LED;
	int filled = prog / PROGRESS_COMPLETE;
	int left = ((prog % PROGRESS_COMPLETE)*LED_MAX)>>8;

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
	return ANIMATION_CONTINUE;
}
static int _animate_factory_test_pattern(const led_color_t * prev, led_color_t * out, void * user_context ){
	int i;
	int counter = *(int*)user_context;
	for(i = 0; i < NUM_LED; i++){
		if( ( (i+counter) % 3 ) == 0 ) {
			out[i] = led_from_rgb(255,255,255);
		} else {
			out[i] = led_from_rgb(0,0,0);
		}
	}
	return ANIMATION_CONTINUE;
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
static bool _reinit_animate_wheel( void * user_context){
	if(user_context){
		wheel_context * ctx = (wheel_context *) user_context;
		ctx->ctr = 0;
		return true;
	}
	return false;
}
static int _animate_wheel(const led_color_t * prev, led_color_t * out, void * user_context ){
	int ret = ANIMATION_CONTINUE;
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
		if(ctx->repeat){
			if(ctx->ctr > (ctx->repeat * 255 - 128)){
				ret = ANIMATION_FADEOUT;
			}
		}

		xSemaphoreGiveRecursive(led_smphr);
	}
	return ret;
}

/*
 * Pubic, call these to set up animationz
 */

static xQueueHandle allocation_queue = 0;

void init_led_animation() {
	//self._sem = xSemaphoreCreateRecursiveMutex();
	allocation_queue = xQueueCreate(4, sizeof(void*));
}
static push_memory_queue(void * new){
	while(xQueueSend(allocation_queue, &new, 0) != pdPASS) {
		void * eldest;
		if( xQueueReceive(allocation_queue, &eldest, portMAX_DELAY) ) {
			void * second_eldest;
			if( xQueueReceive(allocation_queue, &second_eldest, portMAX_DELAY) ) {
				vPortFree(second_eldest);
			}
			push_memory_queue(eldest);
		}
	}
}
int play_pairing_glow( void ){
	int ret;
	uint8_t trippy_base[3] = { 25, 30, 230 };
	uint8_t trippy_range[3] = { 25, 30, 228 }; //last on wraps, but oh well
	user_animation_t anim = (user_animation_t){
		.handler = _animate_trippy,
		.reinit_handler = NULL,
		.context = NULL,
		.reinit_handler = NULL,
		.priority = 0,
		.cycle_time = 30,
		.fadein_time = 30,
		.fadein_elapsed = 0,
		.opt = 0,
	};

	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	ret = led_transition_custom_animation(&anim);
	if( ret > 0 ) {
		memcpy(self.trippy_base, trippy_base, 3);
		memcpy(self.trippy_range, trippy_range, 3);
	}
	xSemaphoreGiveRecursive(led_smphr);
	return ret;
}

int play_led_trippy(uint8_t trippy_base[3], uint8_t trippy_range[3], unsigned int timeout, unsigned int delay, unsigned int fade ){
	int ret;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_trippy,
		.reinit_handler = NULL,
		.context = NULL,
		.reinit_handler = NULL,
		.priority = 1,
		.cycle_time = delay,
		.fadein_time = fade,
		.fadein_elapsed = 0,
		.opt = 0,
	};

	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	ret = led_transition_custom_animation(&anim);
	if( ret > 0 ) {
		memcpy(self.trippy_base, trippy_base, 3);
		memcpy(self.trippy_range, trippy_range, 3);
	}
	xSemaphoreGiveRecursive(led_smphr);
	return ret;

}
int play_led_animation_solid(int a, int r, int g, int b, int repeat, int delay, int priority){
	_animate_solid_ctx * ctx = pvPortMalloc(sizeof(_animate_solid_ctx));
	int ret;

	assert(ctx);

	ctx->color = led_from_rgb(r, g, b);
	ctx->alpha = a;
	analytics_event( "{led: solid, color: %08x, alpha: %d}", ctx->color, ctx->alpha );
	ctx->ctr = 0;
	ctx->repeat = repeat;
	user_animation_t anim = (user_animation_t){
			.handler = _animate_solid,
			.reinit_handler = _reinit_animate_solid,
			.context = ctx,
			.priority = priority,
			.cycle_time = delay,
			.fadein_time = 0,
			.fadein_elapsed = 0,
			.opt = 0,
	};
	ret = led_transition_custom_animation(&anim);
	if( ret > 0 ) {
		push_memory_queue((void*)ctx);
	} else {
		vPortFree(ctx);
	}
	return ret;
}
int play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout){
	int ret;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_progress,
		.reinit_handler = NULL,
		.context = NULL,
		.priority = 1,
		.cycle_time = 20,
		.fadein_time = 0,
		.fadein_elapsed = 0,
		.opt = TRANSITION_WITHOUT_FADE,
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
		.reinit_handler = NULL,
		.context = (void*)&counter,
		.priority = 2,
		.cycle_time = 500,
		.fadein_time = 0,
		.fadein_elapsed = 0,
		.opt = 0,
	};
	ret = led_transition_custom_animation(&anim);
	return ret;
}

int play_led_wheel(int a, int r, int g, int b, int repeat, int delay, int priority){
	int ret;
	led_color_t color = led_from_rgb(r,g,b);
	analytics_event( "{led: wheel, color: %08x, alpha: %d}", color, a );
	color = led_from_brightness( &color, a );

	wheel_context * wheel_ctx =  pvPortMalloc(sizeof(wheel_context));
	assert(wheel_ctx);
	wheel_ctx->ctr = 0;
	wheel_ctx->repeat = repeat;
	wheel_ctx->color = color;

	user_animation_t anim = (user_animation_t){
		.handler = _animate_wheel,
		.reinit_handler = _reinit_animate_wheel,
		.context = wheel_ctx,
		.priority = priority,
		.cycle_time = delay,
		.fadein_time = 0,
		.fadein_elapsed = 0,
		.opt = 0,
	};
	ret = led_transition_custom_animation(&anim);

	if( ret > 0 ) {
		push_memory_queue((void*)wheel_ctx);
	} else {
		vPortFree(wheel_ctx);
	}
	return ret;
}
int play_modulation(int r, int g, int b, int delay, int priority){
	_animate_solid_ctx * ctx = pvPortMalloc(sizeof(_animate_solid_ctx));
	int ret;

	assert(ctx);

	ctx->color = led_from_rgb(r, g, b);
	ctx->alpha = 0;
	analytics_event( "{led: modulation, color: %08x}", ctx->color );
	ctx->ctr = 0;
	ctx->repeat = 0;
	user_animation_t anim = (user_animation_t){
			.handler = _animate_modulation,
			.reinit_handler = _reinit_animate_solid,
			.context = ctx,
			.priority = priority,
			.cycle_time = delay,
			.fadein_time = 0,
			.fadein_elapsed = 0,
			.opt = 0,
	};
	ret = led_transition_custom_animation(&anim);
	if( ret > 0 ) {
		push_memory_queue((void*)ctx);
	} else {
		vPortFree(ctx);
	}
	return ret;
}
void set_modulation_intensity(uint8_t intensity){
	self.alpha_override = intensity;
}
void set_led_progress_bar(uint8_t percent){
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	self.progress_bar_percent =  percent > PROGRESS_COMPLETE?PROGRESS_COMPLETE:percent;
	xSemaphoreGiveRecursive(led_smphr);
}

void stop_led_animation(unsigned int delay, unsigned int fadeout){
	ANIMATE_BLOCKING(led_fade_all_animation(fadeout),delay);
}

int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			led_fade_current_animation();
		}else if(strcmp(argv[1], "+") == 0){
			set_led_progress_bar(self.progress_bar_percent += 5);
		}else if(strcmp(argv[1], "-") == 0){
			set_led_progress_bar(self.progress_bar_percent -= 5);
		}else if(strcmp(argv[1], "trippy") == 0){
			if(argc >= 8){
				uint8_t trippy_base[3] = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4])};
				uint8_t trippy_range[3] = {atoi(argv[5]), atoi(argv[6]), atoi(argv[7])};
				play_led_trippy( trippy_base, trippy_range, portMAX_DELAY, 30,30 );
			}else{
				uint8_t trippy_base[3] = {rand()%120, rand()%120, rand()%120};
				uint8_t trippy_range[3] = {rand()%120, rand()%120, rand()%120};
				play_led_trippy( trippy_base, trippy_range, portMAX_DELAY, 30,30 );
			}
		}else if(strcmp(argv[1], "wheel") == 0){
			play_led_wheel(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 2, 16,1);
		}else if(strcmp(argv[1], "wheelr") == 0){
			play_led_wheel(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 0, 16,1);
		}else if(strcmp(argv[1], "solid") == 0){
			play_led_animation_solid(rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, rand()%LED_MAX, 1,18, 1);
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
