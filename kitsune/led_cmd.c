#include "led_cmd.h"
#include <stdbool.h>
#include <hw_memmap.h>
#include "rom_map.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#define NUM_LED 12
#define LED_GPIO_BIT 0x1
#define LED_GPIO_BASE GPIOA3_BASE

#define LED_LOGIC_HIGH_FAST 0
#define LED_LOGIC_LOW_FAST LED_GPIO_BIT
#define LED_LOGIC_HIGH_SLOW LED_GPIO_BIT
#define LED_LOGIC_LOW_SLOW 0

#if defined(ccs)

#endif

#ifndef minval
#define minval( a,b ) a < b ? a : b
#endif

static EventGroupHandle_t led_events;
static struct{
	int r;
	int g;
	int b;
}user_color_t;

int led_init(void){
	led_events = xEventGroupCreate();
	if (led_events == NULL) {
		return -1;
	}
	return 0;
}

static void led_fast( unsigned int* color ) {
	int i;
	unsigned int * end = color + NUM_LED;

	for( ;; ) {
		for (i = 0; i < 24; ++i) {
			if ((*color << i) & 0x800000 ) {
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
static void led_slow(unsigned int* color) {
	int i;
	unsigned int * end = color + NUM_LED;
	for (;;) {
		for (i = 0; i < 24; ++i) {
			if ((*color << i) & 0x800000) {
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
				UtilsDelay(1);
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
static void led_array(unsigned int * colors) {
	unsigned long ulInt;
	//

	// Temporarily turn off interrupts.
	//
	bool fast = MAP_GPIOPinRead(LED_GPIO_BASE_DOUT, LED_GPIO_BIT_DOUT);
	ulInt = MAP_IntMasterDisable();
	if (fast) {
		led_fast(colors);
	} else {
		led_slow(colors);
	}
	if (!ulInt) {
		MAP_IntMasterEnable();
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
static void led_brightness(unsigned int * colors, unsigned int brightness ) {
	int l;
	unsigned int blue,red,green;

	for (l = 0; l < NUM_LED; ++l) {
		blue = ( colors[l] & ~0xffff00 );
		red = ( colors[l] & ~0xff00ff )>>8;
		green = ( colors[l] & ~0x00ffff )>>16;

		blue = ((brightness * blue)>>8)&0xff;
		red = ((brightness * red)>>8)&0xff;
		green = ((brightness * green)>>8)&0xff;
		colors[l] = (blue) | (red<<8) | (green<<16);
	}
}
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

static void led_to_rgb( unsigned int * c, unsigned int *r, unsigned int* g, unsigned int* b) {
	*b = (( *c & ~0xffff00 ))&0xff;
	*r = (( *c & ~0xff00ff )>>8)&0xff;
	*g = (( *c & ~0x00ffff )>>16)&0xff;
}
static unsigned int led_from_rgb( int r, int g, int b) {
	return (b&0xff) | ((r&0xff)<<8) | ((g&0xff)<<16);
}
static uint32_t wheel_color(int WheelPos, unsigned int color) {
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

static uint32_t wheel(int WheelPos) {
	if (WheelPos < 85) {
		return led_from_rgb(WheelPos * 3, 255 - WheelPos * 3, 0);
	} else if (WheelPos < 170) {
		WheelPos -= 85;
		return led_from_rgb(255 - WheelPos * 3, 0, WheelPos * 3);
	} else {
		WheelPos -= 170;
		return led_from_rgb(0, WheelPos * 3, 255 - WheelPos * 3);
	}
}

#define LED_RESET_BIT 				0x01
#define LED_SOLID_PURPLE_BIT 		0x02
#define LED_ROTATE_PURPLE_BIT 		0x04
#define LED_ROTATE_RAINBOW_BIT		0x08

#define LED_FADE_IN_BIT			0x010
#define LED_FADE_IN_FAST_BIT	0x020
#define LED_FADE_OUT_BIT		0x040
#define LED_FADE_OUT_FAST_BIT	0x080

#define LED_FADE_OUT_ROTATE_BIT 0x100
#define LED_FADE_OUT_STEP_BIT   0x200
#define LED_FADE_IN_STEP_BIT    0x400

#define LED_CUSTOM_COLOR		0x1000


void led_task( void * params ) {
	int i,j;
	unsigned int colors_last[NUM_LED+1];
	memset( colors_last, 0, sizeof(colors_last) );
	led_array( colors_last );

	while(1) {
		EventBits_t evnt;

		evnt = xEventGroupWaitBits(
		                led_events,   /* The event group being tested. */
		                0xffffff,    /* The bits within the event group to wait for. */
		                pdFALSE,        /* all bits should not be cleared before returning. */
		                pdFALSE,       /* Don't wait for both bits, either bit will do. */
		                1000 );/* Wait for any bit to be set. */
		if( evnt & LED_RESET_BIT ) {
			memset( colors_last, 0, sizeof(colors_last) );
			led_array( colors_last );

			xEventGroupClearBits(led_events, LED_RESET_BIT );
		}

		if (evnt & LED_SOLID_PURPLE_BIT) {
			unsigned int colors[NUM_LED + 1];
			int color_to_use = led_from_rgb(132, 0, 255);
			for (i = 0; i <= NUM_LED; ++i) {
				colors[i] = color_to_use;
			}
			led_array(colors);
			memcpy(colors_last, colors, sizeof(colors_last));

			xEventGroupClearBits(led_events, LED_SOLID_PURPLE_BIT );
		}
		if (evnt & LED_ROTATE_PURPLE_BIT) {
			unsigned int colors[NUM_LED + 1];
			int color_to_use = led_from_rgb(132, 0, 255);
			for (i = 0; i <= NUM_LED; ++i) {
				colors[i] = wheel_color(((i * 256 / 12) + j) & 255, color_to_use);
			}
			++j;
			led_array(colors);
			memcpy(colors_last, colors, sizeof(colors_last));

			vTaskDelay(20);
		}
		if (evnt & LED_ROTATE_RAINBOW_BIT) {
			unsigned int colors[NUM_LED + 1];
			int color_to_use = led_from_rgb(132, 0, 255);
			for (i = 0; i <= NUM_LED; ++i) {
				colors[i] = wheel(((i * 256 / 12) + j) & 255);
			}
			++j;
			led_array(colors);
			memcpy(colors_last, colors, sizeof(colors_last));

			vTaskDelay(20);
		}
		if (evnt & LED_FADE_OUT_ROTATE_BIT) {
			unsigned int r, g, b, ro, go, bo;
			unsigned int colors[NUM_LED + 1];
			int color_to_use = led_from_rgb(132, 0, 255);
			for (i = 0; i <= NUM_LED; ++i) {
				colors[i] = wheel_color(((i * 256 / 12) + j) & 255,
						color_to_use);

				led_to_rgb(&colors[i], &r, &g, &b);
				led_to_rgb(&colors_last[i], &ro, &go, &bo);

				r = minval(r, ro);
				g = minval(g, go);
				b = minval(b, bo);

				colors[i] = led_from_rgb(r, g, b);
			}
			++j;
			for (i = 0; i < NUM_LED; i++) {
				led_to_rgb(&colors[i], &r, &g, &b);
				if (!(r < 20 && g < 20 && b < 20)) {
					break;
				}
			}
			if (i == NUM_LED) {
				xEventGroupClearBits(led_events, LED_FADE_OUT_ROTATE_BIT);
				xEventGroupSetBits(led_events,
						LED_FADE_IN_BIT | LED_FADE_IN_FAST_BIT);
			}

			led_array(colors);
			memcpy(colors_last, colors, sizeof(colors_last));

			vTaskDelay(20);
		}
		if (evnt & LED_FADE_IN_BIT) {
			j = 0;
			xEventGroupClearBits(led_events, LED_FADE_IN_BIT);
			xEventGroupSetBits(led_events, LED_FADE_IN_STEP_BIT);
		}
		if (evnt & LED_FADE_IN_STEP_BIT) { //set j to 0 first
			unsigned int colors[NUM_LED + 1];
			int color_to_use = led_from_rgb(132, 0, 255);
			for (i = 0; i <= NUM_LED; i++) {
				colors[i] = color_to_use;
			}
			led_brightness(colors, ++j);
			led_array(colors);
			memcpy(colors_last, colors, sizeof(colors_last));

			if (j > 255) {
				xEventGroupClearBits(led_events, LED_FADE_IN_STEP_BIT | LED_FADE_IN_FAST_BIT);
			}

			if (evnt & LED_FADE_IN_FAST_BIT) {
				vTaskDelay(3);
			} else {
				vTaskDelay(6);
			}
		}
		if (evnt & LED_FADE_OUT_BIT) {
			j = 255;
			xEventGroupClearBits(led_events, LED_FADE_OUT_BIT);
			xEventGroupSetBits(led_events, LED_FADE_OUT_STEP_BIT);
		}
		if (evnt & LED_FADE_OUT_STEP_BIT) {
			unsigned int colors[NUM_LED + 1];
			int color_to_use = led_from_rgb(132, 0, 255);
			for (i = 0; i <= NUM_LED; i++) {
				colors[i] = color_to_use;
			}
			led_brightness(colors, --j);
			led_array(colors);
			memcpy(colors_last, colors, sizeof(colors_last));

			if (j == 0) {
				xEventGroupClearBits(led_events,
						LED_FADE_OUT_STEP_BIT | LED_FADE_OUT_FAST_BIT);
			}

			if (evnt & LED_FADE_OUT_FAST_BIT) {
				vTaskDelay(3);
			} else {
				vTaskDelay(6);
			}
		}
	}
}

int Cmd_led(int argc, char *argv[]) {
	if(argc == 2) {
		int select = atoi(argv[1]);
		xEventGroupClearBits( led_events, 0xffffff );
		xEventGroupSetBits( led_events, select );
	}
	return 0;
}

int Cmd_led_clr(int argc, char *argv[]) {

	xEventGroupClearBits( led_events, 0xffffff );
	xEventGroupSetBits( led_events, LED_RESET_BIT );

	return 0;
}
