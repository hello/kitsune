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

#define LED_LOGIC_HIGH_FAST 0
#define LED_LOGIC_LOW_FAST LED_GPIO_BIT
#define LED_LOGIC_HIGH_SLOW LED_GPIO_BIT
#define LED_LOGIC_LOW_SLOW 0

#define LED_CLAMP_MAX 255
#if defined(ccs)

#endif

#ifndef minval
#define minval( a,b ) a < b ? a : b
#endif


xSemaphoreHandle led_smphr;
static EventGroupHandle_t led_events;
static struct{
	uint8_t r;
	uint8_t g;
	uint8_t b;
}user_color;
unsigned int user_delay;
static int animation_id;

static user_animation_t user_animation;
static user_animation_t prev_user_animation;


static int clamp_rgb(int v, int min, int max){
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

static void led_fast(led_color_t * color ) {
	int i;
	led_color_t * end = &color[NUM_LED];

	for( ;; ) {
		for (i = 0; i < 24; ++i) {
			if ((color->rgb << i) & 0x800000 ) {
				//1
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_FAST);
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_FAST);
				if( i!=23 ) {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");

				} else {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				}
			} else {
				//0
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_HIGH_FAST);
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				MAP_GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_BIT, LED_LOGIC_LOW_FAST);
				if( i!=23 ) {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");

				} else {
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
					__asm( " nop");__asm( " nop");__asm( " nop");__asm( " nop");
				}
			}
		}
		if( ++color > end ) {
			return;
		}
	}
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

#define LED_GPIO_BASE_DOUT GPIOA2_BASE
#define LED_GPIO_BIT_DOUT 0x80
static void led_array(led_color_t * colors, int delay) {
	unsigned long ulInt;
	//

	// Temporarily turn off interrupts.
	//
	bool fast = MAP_GPIOPinRead(LED_GPIO_BASE_DOUT, LED_GPIO_BIT_DOUT);

	vTaskDelay(delay);
	ulInt = MAP_IntMasterDisable();
	if (fast) {
		led_fast(colors);
	} else {
		led_slow(colors);
	}
	if (!ulInt) {
		MAP_IntMasterEnable();
	}
	vTaskDelay(0);
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
	vTaskDelay(10000);
	while(1) {
		xEventGroupWaitBits(
				led_events,   /* The event group being tested. */
				LED_IDLE_BIT,    /* The bits within the event group to wait for. */
				pdFALSE,        /* all bits should not be cleared before returning. */
				pdFALSE,       /* Don't wait for both bits, either bit will do. */
				portMAX_DELAY );/* Wait for any bit to be set. */
		xEventGroupSetBits(led_events,LED_RESET_BIT);
		vTaskDelay(10000);
	}
}
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
static void
_reset_user_animation(user_animation_t * anim){
	UARTprintf("resetting animation %x\n", anim->handler );
	anim->priority = 0xff;
}

static int get_cycle_time() {
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	int cycle_time = user_animation.cycle_time;
	xSemaphoreGiveRecursive( led_smphr );
	return cycle_time;
}

void led_task( void * params ) {
	int j;
	led_color_t colors_last[NUM_LED+1];
	memset( colors_last, 0, sizeof(colors_last) );
	_reset_user_animation(&user_animation);
	led_smphr = xSemaphoreCreateRecursiveMutex();
	assert(led_smphr);

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
			UARTprintf("reset bit for %x\n", user_animation.handler );
			//if the last one wasn't a stop and was something different from the current one...
			if( user_animation.priority != 0 &&  user_animation.priority != 0xff &&
					prev_user_animation.handler != user_animation.handler &&
					prev_user_animation.handler != NULL ) {
				UARTprintf("reverting to animation %x\n", prev_user_animation.handler );
				led_transition_custom_animation(&prev_user_animation);
			} else {
				_reset_user_animation(&user_animation);
			}
			xSemaphoreGiveRecursive(led_smphr);
			UARTprintf("done with reset\n" );
			continue;
		}
		if(evnt & LED_CUSTOM_TRANSITION){
			led_color_t colors[NUM_LED + 1];
			int i;
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
			for(i = 0; i < NUM_LED; i++){
				_transition(&colors[i], &colors_last[i], &user_animation.initial_state[i]);
			}
			xSemaphoreGiveRecursive( led_smphr );
			led_array(colors, get_cycle_time());
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);

			memcpy(colors_last,colors, sizeof(colors_last));
			if(0 == memcmp(user_animation.initial_state,colors,sizeof(user_animation.initial_state))){
				xEventGroupClearBits(led_events,LED_CUSTOM_TRANSITION);
				xEventGroupSetBits(led_events, LED_CUSTOM_ANIMATION_BIT);

				UARTprintf("transition done\n");
			}
			xSemaphoreGiveRecursive( led_smphr );
		}
		if(evnt & LED_CUSTOM_ANIMATION_BIT){
			led_color_t colors[NUM_LED + 1];
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
			if(user_animation.handler){
				if(user_animation.handler(colors_last, colors,user_animation.context )){
					memcpy(colors_last,colors, sizeof(colors_last));
					//delay capped at 500 ms to improve task responsiveness
					xSemaphoreGiveRecursive( led_smphr );
					led_array(colors, get_cycle_time());
					xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
				}else{
					vTaskDelay( user_animation.cycle_time );
					xEventGroupClearBits(led_events,LED_CUSTOM_ANIMATION_BIT);
					//xEventGroupSetBits(led_events,LED_RESET_BIT);
					j = 255;
					xEventGroupSetBits(led_events, LED_FADE_OUT_STEP_BIT );  // always fade out animation
					UARTprintf("animation done %x\n", user_animation.handler);
				}
			}else{
				vTaskDelay( user_animation.cycle_time );
				xEventGroupClearBits(led_events,LED_CUSTOM_ANIMATION_BIT);
				xEventGroupSetBits(led_events,LED_RESET_BIT);
				UARTprintf("animation no handler\n");
			}
			xSemaphoreGiveRecursive( led_smphr );
		}
		if (evnt & LED_FADE_OUT_STEP_BIT) {
			led_color_t colors[NUM_LED + 1];
			xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
			if(user_animation.handler
					&& user_animation.handler(colors_last, colors, user_animation.context)){
				ledcpy(colors_last, colors, NUM_LED);
				j-=QUANT_FACTOR;
				if (j < 0) {
					xEventGroupClearBits(led_events, 0xffffff);
					xEventGroupSetBits(led_events,LED_RESET_BIT);
					memset(colors, 0, sizeof(colors));
					memcpy(colors_last, colors, sizeof(colors_last));
					UARTprintf("led faded out\n");
				} else {
					led_brightness_all(colors, j);
					led_array(colors, get_cycle_time());
				}
			}
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
			r = clamp_rgb(atoi(argv[2]), 0, LED_CLAMP_MAX);
			g = clamp_rgb(atoi(argv[3]), 0, LED_CLAMP_MAX);
			b = clamp_rgb(atoi(argv[4]), 0, LED_CLAMP_MAX);
			ANIMATE_BLOCKING(play_led_animation_stop(33),500);
			LOGF("Setting colors R: %d, G: %d, B: %d \r\n", r, g, b);
			play_led_animation_solid(LED_MAX, r,g,b,1, 18);
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
			play_led_animation_solid(a, r,g,b, rp,ud);
		}else{
			play_led_wheel(a, r,g,b,rp,ud);
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

unsigned int led_delay( unsigned int dly ) {
	return (255 / QUANT_FACTOR + 1) * dly;
}

int led_set_color(uint8_t alpha, uint8_t r, uint8_t g, uint8_t b,
		int fade_in, int fade_out,
		unsigned int ud,
		int rot) {
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	user_color.r = clamp_rgb(r, 0, LED_CLAMP_MAX) * alpha / 0xFF;
	user_color.g = clamp_rgb(g, 0, LED_CLAMP_MAX) * alpha / 0xFF;
	user_color.b = clamp_rgb(b, 0, LED_CLAMP_MAX) * alpha / 0xFF;
	user_delay = ud;
    LOGI("Setting colors R: %d, G: %d, B: %d \r\n", user_color.r, user_color.g, user_color.b);
	xEventGroupClearBits( led_events, 0xffffff );

	xSemaphoreGiveRecursive(led_smphr);
	return 0;
}

int led_transition_custom_animation(const user_animation_t * user){
	user_animation_t temp;
	int ret = -1;
	if(!user){
		UARTprintf("no user\n");
		return ret;
	}else{
		xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
		UARTprintf("priority check %x %x\n", user->handler, user_animation.handler );
		if(user->priority <= user_animation.priority){

			UARTprintf("new animation %x %x\n", user->handler, user->priority );
			temp = *user;
			UARTprintf("t1 animation %x %x\n", temp.handler, temp.priority );

    		UARTprintf("saving new %x old %x older %x\n", user->handler, user_animation.handler, prev_user_animation.handler );
			prev_user_animation = user_animation;

			UARTprintf("t2 animation %x %x\n", temp.handler, temp.priority );
			user_animation = temp;
			UARTprintf("set animation %x %x\n", user_animation.handler, user_animation.priority );

			user_animation.cycle_time = clamp_rgb(user_animation.cycle_time,10,500);
			UARTprintf("cycle time %d\n", user_animation.cycle_time );

			ret = ++animation_id;
			xEventGroupClearBits( led_events, 0xffffff );
			xEventGroupSetBits( led_events, LED_CUSTOM_TRANSITION );
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
static uint8_t _rgb[3];

void led_get_user_color(uint8_t* out_red, uint8_t* out_green, uint8_t* out_blue)
{
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	if(out_red){
		*out_red = _rgb[0];
	}

	if(out_green){
		*out_green = _rgb[1];
	}

	if(out_blue){
		*out_blue = _rgb[2];
	}
	xSemaphoreGiveRecursive(led_smphr);
}

void led_set_user_color(uint8_t red, uint8_t green, uint8_t blue)
{
	xSemaphoreTakeRecursive(led_smphr, portMAX_DELAY);
	_rgb[0] = red;
	_rgb[1] = green;
	_rgb[2] = blue;
	xSemaphoreGiveRecursive(led_smphr);
}
