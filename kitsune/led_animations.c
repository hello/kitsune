#include <hw_memmap.h>
#include <stdlib.h>
#include "rom_map.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"

#include "uart_logger.h"

#include "led_animations.h"
struct _colors{
		int r,g,b;
};
static struct{
	int counter;
	volatile bool sig_continue;
	struct _colors colors[NUM_LED],prev_colors[NUM_LED];
	int progress_bar_percent;
	unsigned int dly;
	xSemaphoreHandle _sem;
	uint8_t trippy_base[3];
	uint8_t trippy_range[3];
}self;

extern void led_unblock();
extern void led_block();
extern void led_set_is_sync(int is_sync);

static bool lock() {
//	LOGI("TAKE self._sem\n");
	return xSemaphoreTakeRecursive(self._sem, portMAX_DELAY) == pdPASS;
}
static void unlock() {
//	LOGI("GIVE self._sem\n");
	xSemaphoreGiveRecursive(self._sem);
}

static bool _signal_start_animation(void) {
	lock();

	LOGI("Start animation\n");
	self.counter = 0;
	self.sig_continue = true; //careful, to set this true requires both semaphores

	unlock();
	return true;
}

static bool _reach_color(int * v, int target){
	if(*v == target){
		return true;
	}else if(*v < target){
		*v = *v + 1;
	}else{
		*v = *v - 1;
	}
	return false;
}
static int _new_random_color(uint8_t range, uint8_t base){
	return ((unsigned int)rand()) % range + base;
}
static bool _animate_solid(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	int i;
	bool sig_continue;
	lock();
	for(i = 0; i < NUM_LED; i++){
		out_r[i] = self.colors[i].r;
		out_g[i] = self.colors[i].g;
		out_b[i] = self.colors[i].b;
	}
	sig_continue = self.sig_continue;
	unlock();
	return sig_continue;
}
static bool _animate_trippy(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	bool sig_continue;
	static int scaler = 100;
	lock();
	for(i = 0; i < NUM_LED; i++){
		if(_reach_color(&self.prev_colors[i].r, self.colors[i].r)){
			self.colors[i].r = _new_random_color(self.trippy_range[0], self.trippy_base[0]);
		}
		if(_reach_color(&self.prev_colors[i].g, self.colors[i].g)){
			self.colors[i].g = _new_random_color(self.trippy_range[1], self.trippy_base[1]);
		}
		if(_reach_color(&self.prev_colors[i].b, self.colors[i].b)){
			self.colors[i].b = _new_random_color(self.trippy_range[2], self.trippy_base[2]);
		}

		out_r[i] = self.prev_colors[i].r * ((unsigned int)(scaler)) / 100;
		out_g[i] = self.prev_colors[i].g * ((unsigned int)(scaler)) / 100;
		out_b[i] = self.prev_colors[i].b * ((unsigned int)(scaler)) / 100;

	}
	*out_delay = self.dly;

	sig_continue = self.sig_continue;
	unlock();
	return sig_continue;
}
static bool _animate_progress(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	bool sig_continue;
	lock();
	int prog = self.progress_bar_percent * rgb_array_size;
	int filled = prog / 100;
	int left = ((prog % 100)*254)>>8;

	int i;
	for(i = 0; i < filled && i < rgb_array_size; i++){
		out_r[i] = self.colors[0].r;
		out_g[i] = self.colors[0].g;
		out_b[i] = self.colors[0].b;
	}
	if(filled < rgb_array_size ){
		out_r[filled] = ((self.colors[0].r * left)>>8)&0xff;
		out_g[filled] = ((self.colors[0].g * left)>>8)&0xff ;
		out_b[filled] = ((self.colors[0].b * left)>>8)&0xff ;
	}
	*out_delay = self.dly;
	sig_continue = self.sig_continue;
	unlock();
	return sig_continue;
}
static bool _animate_factory_test_pattern(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	bool sig_continue;
	lock();

	int i;
	for(i = 0; i < rgb_array_size; i++){
		if( ( (i+self.counter) % 3 ) == 0 ) {
			out_r[i] = 255;
			out_g[i] = 255;
			out_b[i] = 255;
		} else {
			out_r[i] = 0;
			out_g[i] = 0;
			out_b[i] = 0;
		}
	}
	++self.counter;
	*out_delay = self.dly;
	sig_continue = self.sig_continue;
	unlock();
	return sig_continue;
}

/*
 * Pubic:
 */

void init_led_animation() {
	self._sem = xSemaphoreCreateRecursiveMutex();
}

bool play_led_trippy(uint8_t trippy_base[3], uint8_t trippy_range[3], unsigned int timeout){
	int i;
	user_animation_t anim = (user_animation_t){
		.handler = _animate_trippy,
		.context = NULL,
		.priority = 1,
		.initial_state = {0},
	};
	memcpy(self.trippy_base, trippy_base, 3);
	memcpy(self.trippy_range, trippy_range, 3);

	for(i = 0; i < NUM_LED; i++){
		self.colors[i].r = _new_random_color(trippy_range[0],trippy_base[0]);
		self.colors[i].g = _new_random_color(trippy_range[1],trippy_base[1]);
		self.colors[i].b = _new_random_color(trippy_range[2],trippy_base[2]);
		anim.initial_state[i] = led_from_rgb(
				self.colors[i].r,
				self.colors[i].g,
				self.colors[i].b);
		self.prev_colors[i] = self.colors[i];
	}
	self.dly = 15;
	//led_start_custom_animation(_animate_trippy, NULL);
	led_transition_custom_animation(&anim);
	_signal_start_animation();
	return true;

}
bool play_led_animation_solid(int r, int g, int b){
	int i;
	user_animation_t anim = (user_animation_t){
			.handler = _animate_solid,
			.context = NULL,
			.priority = 2,
			.initial_state = {0},
	};
	for(i = 0; i < NUM_LED; i++){
			self.colors[i].r = r;
			self.colors[i].g = g;
			self.colors[i].b = b;
			anim.initial_state[i] = led_from_rgb(
					self.colors[i].r,
					self.colors[i].g,
					self.colors[i].b);
			self.prev_colors[i] = self.colors[i];
	}
	self.dly = 33;
	led_transition_custom_animation(&anim);
	_signal_start_animation();
	return true;
}
bool play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout){
	user_animation_t anim = (user_animation_t){
		.handler = _animate_solid,
		.context = NULL,
		.priority = 2,
		.initial_state = {0},
	};
	self.colors[0] = (struct _colors){r, g, b};
	self.progress_bar_percent = 0;
	self.dly = 20;
	led_transition_custom_animation(&anim);
	_signal_start_animation();
	return true;
}
void set_led_progress_bar(uint8_t percent){
	lock();
	self.progress_bar_percent =  percent > 100?100:percent;
	unlock();
}

void stop_led_animation(unsigned int delay){
	lock();
	if(self.sig_continue){
		self.sig_continue = false;
		self.dly = delay;
		led_fadeout(self.dly);

	}
	unlock();
}

int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			stop_led_animation(1);
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
			led_set_color(100, 88,0,150, 1, 1, 18, 1 );
		}else if(strcmp(argv[1], "solid") == 0){
			play_led_animation_solid(rand()%120, rand()%120, rand()%120);
		}else if(strcmp(argv[1], "prog") == 0){
			play_led_progress_bar(20, 20, 20, 0, portMAX_DELAY);
		}
		return 0;
	}else{
		return -1;
	}
	//play_led_trippy( portMAX_DELAY );
	//play_led_progress_bar(20, 20, 20, 0, portMAX_DELAY);
	return 0;
}
bool factory_led_test_pattern(unsigned int timeout) {
		self.dly = 500;
		led_start_custom_animation(_animate_factory_test_pattern, NULL);
		_signal_start_animation();
		return true;
}
