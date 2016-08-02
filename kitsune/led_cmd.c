#include "led_cmd.h"
#include <hw_memmap.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"
#include <string.h>


#include "hw_types.h"
#include "hw_ints.h"
#include "hw_wdt.h"
#include "wdt.h"
#include "wdt_if.h"
#include "rom.h"
#include "rom_map.h"

#include "gpio.h"
#include "interrupt.h"
#include "uartstdio.h"
#include "utils.h"

#include "led_animations.h"
#include "kit_assert.h"

//debug outputs
#define UARTprintf(...)

#define LED_GPIO_BIT 0x1
#define LED_GPIO_BASE GPIOA3_BASE

#define LED_LOGIC_HIGH_SLOW LED_GPIO_BIT
#define LED_LOGIC_LOW_SLOW 0

#define LED_CLAMP_MAX 255
#if defined(ccs)

#endif


#ifndef minval
#define minval( a,b ) a < b ? a : b
#endif
#define ANIMATION_HISTORY_SIZE 3

xSemaphoreHandle led_smphr;
static EventGroupHandle_t led_events;
static struct{
	uint8_t r;
	uint8_t g;
	uint8_t b;
}user_color;
static int animation_id;
static int fade_alpha;

static user_animation_t user_animation;
static user_animation_t fadeout_animation;
static user_animation_t hist[ANIMATION_HISTORY_SIZE];
static int hist_idx;



static int _clamp(int v, int min, int max){
	if(v >= min && v <=max){
		return v;
	}else if(v > max){
		return max;
	}else if(v < min){
		return min;
	}else{
		return 0;
	}
}
int led_init(void){
	led_events = xEventGroupCreate();
	if (led_events == NULL) {
		return -1;
	}
	return 0;
}

static void led_slow(led_color_t * color) {
	int i;
	led_color_t * end = &color[NUM_LED];

	for (;;) {
		for (i = 0; i < 24; ++i) {
			if ((color->rgb << i) & 0x800000) {
				//1
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_SLOW);
				UtilsDelay(5);
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_SLOW);
				if (i != 23) {
					UtilsDelay(5);
				} else {
					UtilsDelay(4);
				}

			} else {
				//0
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_SLOW);
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_SLOW);
				if (i != 23) {
					UtilsDelay(5);
				} else {
					UtilsDelay(2);
				}
			}
		}
		if (++color > end) {
			return;
		}
	}
}

static void set_low() {
	MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_SLOW);
}

static void led_array(led_color_t * colors, int delay) {
	unsigned long ulInt;
	//

}
static void led_brightness_all(led_color_t * colors, unsigned int brightness ) {
	int l;
	for (l = 0; l < NUM_LED; ++l) {
		colors[l] = led_from_brightness(&colors[l], brightness);
	}
}
#if 0
static void led_add_intensity(unsigned int * colors, int intensity ) {
	int l;
	int blue,red,green;

	for (l = 0; l < NUM_LED; ++l) {
		blue = ( colors[l] & ~0xffff00 );
		red = ( colors[l] & ~0xff00ff )>>8;
		green = ( colors[l] & ~0x00ffff )>>16;

		blue = blue + intensity < 0 ? 0 : blue + intensity;
		red = red + intensity < 0 ? 0 : red + intensity;
		green = green + intensity < 0 ? 0 : green + intensity;

		blue = blue > 0xff ? 0xff : blue&0xff;
		red = red > 0xff ? 0xff : red&0xff;
		green = green > 0xff ? 0xff : green&0xff;

		colors[l] = (blue) | (red<<8) | (green<<16);
	}
}

static void led_ccw( unsigned int * colors) {
	int l;
	for (l = 0; l < NUM_LED-2; ++l) {
		int temp = colors[l];
		colors[l] = colors[l + 1];
		colors[l + 1] = temp;
	}
}
static void led_cw( unsigned int * colors) {
	int l;
	for (l = NUM_LED-2; l > -1; --l) {
		int temp = colors[l];
		colors[l] = colors[l + 1];
		colors[l + 1] = temp;
	}
}
#endif
void led_to_rgb(const led_color_t * c, unsigned int *r, unsigned int* g, unsigned int* b) {
	*b = (( c->rgb & ~0xffff00 ))&0xff;
	*r = (( c->rgb & ~0xff00ff )>>8)&0xff;
	*g = (( c->rgb & ~0x00ffff )>>16)&0xff;
}
led_color_t led_from_rgb( int r, int g, int b) {
	led_color_t ret = (led_color_t){
			.rgb =  (b&0xff) | ((r&0xff)<<8) | ((g&0xff)<<16),
	};
	return ret;
}
led_color_t led_from_brightness(const led_color_t * c, unsigned int br){
		unsigned int r,g,b;
		led_to_rgb(c, &r,&g,&b);
		b = ((br * b)>>8)&0xff;
		r = ((br * r)>>8)&0xff;
		g = ((br * g)>>8)&0xff;
		return led_from_rgb(r,g,b);
}
void ledset(led_color_t * dst, led_color_t src, int copies){
	int i;
	for(i = 0; i < copies; i++){
		dst[i] = src;
	}
}
void ledcpy(led_color_t * dst, const led_color_t * src, int num){
	int i;
	for(i = 0; i < num; i++){
		dst[i] = src[i];
	}
}

#define LED_RESET_BIT 		0x01
#define LED_IDLE_BIT        0x08

#define LED_FADE_OUT_STEP_BIT   0x200

#define LED_CUSTOM_ANIMATION_BIT  0x1000
#define LED_CUSTOM_TRANSITION	  0x2000

#define QUANT_FACTOR 6

void led_idle_task( void * params ) {
	set_low();
	vTaskDelay(10000);
	while(1) {
		xEventGroupWaitBits(
				led_events,   /* The event group being tested. */
				LED_IDLE_BIT,    /* The bits within the event group to wait for. */
				pdFALSE,        /* all bits should not be cleared before returning. */
				pdFALSE,       /* Don't wait for both bits, either bit will do. */
				portMAX_DELAY );/* Wait for any bit to be set. */
		led_color_t colors_last[NUM_LED+1];
		memset( colors_last, 0, sizeof(colors_last) );
		led_array( colors_last, 0 );
		vTaskDelay(10000);
	}
}
#if 0
static int _transition_color(int from, int to, int quant){
	int diff = to - from;
	if(diff <= quant && diff >= -quant){
		return to;
	}else{
		if(diff > 0){
			return from + quant;
		}
		return from - quant;
	}
}
static void _transition(led_color_t * out, led_color_t * from, led_color_t * to){
	unsigned int r0,g0,b0,r1,g1,b1;
	led_to_rgb(from, &r0, &g0, &b0);
	led_to_rgb(to, &r1,&g1,&b1);
	*out = led_from_rgb(
			_transition_color((int)r0, (int)r1, 1),
			_transition_color((int)g0, (int)g1, 1),
			_transition_color((int)b0, (int)b1, 1));

}
#endif

static void
_reset_animation_priority(user_animation_t * anim){
//	DISP("resetting animation %x\n", anim->handler );
	anim->priority = 0xff;
}


static bool
_hist_push(const user_animation_t * anim){
	if(hist_idx <ANIMATION_HISTORY_SIZE && anim->priority != 0xff ){
		if( hist_idx == 0 || ( hist[hist_idx-1].handler != anim->handler ) ) {
			DISP("Push anim %x i %d\r\n", anim->handler, hist_idx );
			hist[hist_idx++] = *anim;
			return true;
		}
	}
	return false;
}
static bool
_hist_pop(user_animation_t * out_anim){
	if(hist_idx > 0 && hist_idx <= ANIMATION_HISTORY_SIZE ){
		DISP("Pop  anim %x i %d\r\n", hist[hist_idx-1], hist_idx-1 );
		*out_anim = hist[--hist_idx];
		if( out_anim->reinit_handler ) {
			out_anim->reinit_handler(out_anim->context);
		}
		return true;
	}
	return false;
}
#if 0
static user_animation_t *
_hist_peep() {
	if(hist_idx > 0 && hist_idx <= ANIMATION_HISTORY_SIZE ){
		return &hist[hist_idx-1];
	}
	return NULL;
}
#endif

static int
_hist_flush(void){
	hist_idx = 0;
	_reset_animation_priority(&user_animation);
	_reset_animation_priority(&fadeout_animation);
	DISP("flush anim\r\n" );
	return hist_idx;
}

static void
_start_fade_out(){
	fade_alpha = 255;
	xEventGroupClearBits(led_events,LED_CUSTOM_ANIMATION_BIT);
	xEventGroupSetBits(led_events, LED_FADE_OUT_STEP_BIT );  // always fade out animation
}
static void
_fade_out_for_new() {
	fade_alpha = 255;
	xEventGroupClearBits(led_events,LED_CUSTOM_ANIMATION_BIT);
	xEventGroupSetBits( led_events, LED_FADE_OUT_STEP_BIT | LED_CUSTOM_TRANSITION );
}
static int get_cycle_time() {
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	int cycle_time = user_animation.cycle_time;
	xSemaphoreGiveRecursive( led_smphr );
	return cycle_time;
}
static void
_start_animation(void){
	xEventGroupClearBits( led_events, 0xffffff );
	xEventGroupSetBits(led_events,LED_CUSTOM_ANIMATION_BIT);

}
void led_task( void * params ) {
	led_color_t colors_last[NUM_LED+1];
	memset( colors_last, 0, sizeof(colors_last) );
	_reset_animation_priority(&user_animation);
	_reset_animation_priority(&fadeout_animation);
	led_smphr = xSemaphoreCreateRecursiveMutex();
	assert(led_smphr);

	xEventGroupSetBits(led_events, LED_IDLE_BIT );

	while(1) {
		EventBits_t evnt;

		evnt = xEventGroupWaitBits(
		                led_events,   /* The event group being tested. */
		                0xfffff7,    /* The bits within the event group to wait for. */
		                pdFALSE,        /* all bits should not be cleared before returning. */
		                pdFALSE,       /* Don't wait for both bits, either bit will do. */
		                portMAX_DELAY );/* Wait for any bit to be set. */

		MAP_WatchdogIntClear(WDT_BASE); //clear wdt... this task spends a lot of time with interrupts disabled....
		//UARTprintf("%x\t%x\n", user_animation.handler, evnt );

		if( evnt & LED_RESET_BIT ) {
			memset( colors_last, 0, sizeof(colors_last) );
			led_array( colors_last, get_cycle_time() );
			xEventGroupClearBits(led_events, 0xffffff );
			xEventGroupSetBits(led_events, LED_IDLE_BIT );
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
			DISP("reset bit for %x\n", user_animation.handler );
			//if the last one was something different from the current one and the current one didn't cancel itself
			if( _hist_pop(&user_animation)  ){
					_start_animation();
			}else{
					_reset_animation_priority(&user_animation);
			}


			xSemaphoreGiveRecursive(led_smphr);
			DISP("done with reset\n" );
			continue;
		}
		if(evnt & LED_CUSTOM_ANIMATION_BIT){
			led_color_t colors[NUM_LED + 1];
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
			if(user_animation.handler){
				int animation_result = user_animation.handler(colors_last, colors,user_animation.context );
				if( animation_result == ANIMATION_CONTINUE ) {
					ledcpy(colors_last, colors, NUM_LED);
					//fade in independently of animation rate...
					if( user_animation.fadein_time != 0 &&
						user_animation.fadein_elapsed < user_animation.fadein_time ) {
						user_animation.fadein_elapsed += get_cycle_time();
						led_brightness_all(colors, LED_MAX * user_animation.fadein_elapsed / user_animation.fadein_time );
					}
					//delay capped at 500 ms to improve task responsiveness
					xSemaphoreGiveRecursive( led_smphr );
					led_array(colors, get_cycle_time());
					xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
				}else if( animation_result == ANIMATION_FADEOUT ){
					vTaskDelay( user_animation.cycle_time );
					_start_fade_out();
					DISP("animation done %x\n", user_animation.handler);
					_reset_animation_priority(&user_animation);
				} else if( animation_result == ANIMATION_STOP ) {
					xEventGroupClearBits(led_events, 0xffffff);
					xEventGroupSetBits(led_events,LED_RESET_BIT);
				}
			}else{
				vTaskDelay( user_animation.cycle_time );
				xEventGroupClearBits(led_events,LED_CUSTOM_ANIMATION_BIT);
				xEventGroupSetBits(led_events,LED_RESET_BIT);
				DISP("animation no handler\n");
			}
			xSemaphoreGiveRecursive( led_smphr );
		}
		if (evnt & ( LED_FADE_OUT_STEP_BIT | LED_CUSTOM_TRANSITION ) ) {
			led_color_t colors[NUM_LED + 1];
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);

			if( user_animation.fadein_time != 0 &&
				user_animation.fadein_elapsed < user_animation.fadein_time ) {
				led_brightness_all(colors_last, LED_MAX * user_animation.fadein_elapsed / user_animation.fadein_time );
				user_animation.fadein_time = 0;
			}
			ledcpy(colors, colors_last, NUM_LED);

			if( evnt & LED_CUSTOM_TRANSITION ) {
				if( fadeout_animation.priority != 0xff && fadeout_animation.handler ){
					if( fadeout_animation.handler(colors_last, colors, fadeout_animation.context) == ANIMATION_STOP ) {
						fade_alpha = 0;
					}
				}
			} else {
				if(user_animation.handler){
					if (user_animation.handler(colors_last, colors,user_animation.context ) == ANIMATION_STOP) {
						fade_alpha = 0;
					}
				}
			}
			fade_alpha-=QUANT_FACTOR;
			if (fade_alpha < 0) {
				_reset_animation_priority(&fadeout_animation);
				if( evnt & LED_CUSTOM_TRANSITION ) {
					xEventGroupClearBits(led_events,LED_CUSTOM_TRANSITION);
					xEventGroupClearBits(led_events,LED_FADE_OUT_STEP_BIT);
					xEventGroupSetBits(led_events, LED_CUSTOM_ANIMATION_BIT);
					DISP("transition done\n");
				} else {
					xEventGroupClearBits(led_events, 0xffffff);
					xEventGroupSetBits(led_events,LED_RESET_BIT);
				}
				ledset(colors, led_from_rgb(0,0,0), NUM_LED);
				//DISP("led faded out\n");
			} else {
				led_brightness_all(colors, fade_alpha);
				led_array(colors, get_cycle_time());
				//DISP("led fading\n");
			}
			ledcpy(colors_last, colors, NUM_LED);
			xSemaphoreGiveRecursive( led_smphr );
		}
	}
}

int led_ready() {
	//make sure the thread isn't doing something else...
	if( xEventGroupGetBits( led_events ) & (LED_CUSTOM_TRANSITION | LED_CUSTOM_ANIMATION_BIT | LED_FADE_OUT_STEP_BIT ) ) {
		return -1;
	}
	return 0;
}
bool led_is_idle(unsigned int wait){
	EventBits_t evnt;
	evnt = xEventGroupWaitBits(
					led_events,   /* The event group being tested. */
					LED_IDLE_BIT,    /* The bits within the event group to wait for. */
					pdFALSE,        /* all bits should not be cleared before returning. */
					pdFALSE,       /* Don't wait for both bits, either bit will do. */
					wait );/* Wait for limited time. */
	return (evnt & LED_IDLE_BIT) != 0;
}

int Cmd_led(int argc, char *argv[]) {
	if(argc == 2 && strcmp(argv[1], "stop") == 0){
		stop_led_animation(1,33);
		return 0;
	}
	if(argc == 2) {
		int select = atoi(argv[1]);
		xEventGroupClearBits( led_events, 0xffffff );
		xEventGroupSetBits( led_events, select );
	}else if(argc == 3){
		if(strcmp(argv[1], "color") == 0 && argc >= 5){
			unsigned int r,g,b;
			r = _clamp(atoi(argv[2]), 0, LED_CLAMP_MAX);
			g = _clamp(atoi(argv[3]), 0, LED_CLAMP_MAX);
			b = _clamp(atoi(argv[4]), 0, LED_CLAMP_MAX);
			ANIMATE_BLOCKING(led_fade_all_animation(-1),500);
			LOGF("Setting colors R: %d, G: %d, B: %d \r\n", r, g, b);
			play_led_animation_solid(LED_MAX, r,g,b,1, 18,1);
		}
	} else if( argc > 3){
		int r,g,b,a,rp/*,fi,fo*/,ud,rot;
		r = atoi(argv[1]);
		g = atoi(argv[2]);
		b = atoi(argv[3]);
		a = atoi(argv[4]);
		/*fi = atoi(argv[4]);*/
		/*fo = atoi(argv[5]);*/
		rp = atoi(argv[5]);
		ud = atoi(argv[6]);
		rot = atoi(argv[7]);
		LOGF("R: %d, G: %d, B: %d A:%d Rot:%d\r\n", r, g, b, a, rot);
		if(rot){
			play_led_animation_solid(a, r,g,b, rp,ud,1);
		}else{
			play_led_wheel(a, r,g,b,rp,ud,1);
		}
	} else {
		factory_led_test_pattern(portMAX_DELAY);
	}

	return 0;
}

int Cmd_led_clr(int argc, char *argv[]) {
	xEventGroupSetBits( led_events, LED_RESET_BIT );

	return 0;
}

int led_set_color(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b,
		int fade_in, int fade_out,
		unsigned int ud,
		int rot) {
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	user_color.r = _clamp(r, 0, LED_CLAMP_MAX) * alpha / 0xFF;
	user_color.g = _clamp(g, 0, LED_CLAMP_MAX) * alpha / 0xFF;
	user_color.b = _clamp(b, 0, LED_CLAMP_MAX) * alpha / 0xFF;
    LOGI("Setting colors R: %d, G: %d, B: %d \r\n", user_color.r, user_color.g, user_color.b);
	xEventGroupClearBits( led_events, 0xffffff );

	xSemaphoreGiveRecursive(led_smphr);
	return 0;
}

int led_transition_custom_animation(const user_animation_t * user){
	int ret = -1;
	if(!user){
		DISP("no user\n");
		return ret;
	}else{
		xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
		DISP("priority check %x %x\n", user->handler, user_animation.handler );
		if( user->priority <= user_animation.priority ){
			if( user->handler != user_animation.handler ) { //don't push repeats
				_hist_push(&user_animation);
			}
			fadeout_animation = user_animation;
			user_animation = *user;

			if( !led_is_idle(0) && !( fadeout_animation.opt & TRANSITION_WITHOUT_FADE ) ) {
				_fade_out_for_new();
			} else {
				_start_animation();
			}
			ret = ++animation_id;
		}else{
			ret = -2;
		}
		xSemaphoreGiveRecursive(led_smphr);
		return ret;
	}
}
int led_get_animation_id(void){
	return animation_id;
}
static led_color_t room_color[2] = {{0x00fe00},{0x00fe00}};

void led_get_user_color(unsigned int* out_red, unsigned int* out_green, unsigned int* out_blue, bool light)
{
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	led_to_rgb(&room_color[light], out_red, out_green, out_blue );
	LOGI("using %d colors %x\n", light, room_color[light]);
	xSemaphoreGiveRecursive(led_smphr);
}

void led_set_user_color(unsigned int red, unsigned int green, unsigned int blue, bool light)
{
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	room_color[light] = led_from_rgb( red, green, blue );
	xSemaphoreGiveRecursive(led_smphr);
}
int led_fade_current_animation(void){
	int ret = 0;
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	if( xEventGroupGetBits( led_events ) & (LED_CUSTOM_TRANSITION | LED_CUSTOM_ANIMATION_BIT) ) {
		_start_fade_out();
	}
	ret = animation_id;
	xSemaphoreGiveRecursive(led_smphr);
	return ret;
}
int led_fade_all_animation(int fadeout){
	int ret = 0;
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	if( fadeout >=  0 ) {
		user_animation.cycle_time = fadeout;
	}
	_hist_flush();
	ret = led_fade_current_animation();
	xSemaphoreGiveRecursive(led_smphr);
	return ret;
}
void flush_animation_history() {
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	_hist_flush();
	xSemaphoreGiveRecursive(led_smphr);
}
