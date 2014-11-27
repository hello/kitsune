#include "led_cmd.h"
#include <hw_memmap.h>
#include <stdlib.h>
#include "rom_map.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "semphr.h"

#include "led_animations.h"
struct _colors{
		int r,g,b;
};
static struct{
	int counter;
	volatile bool sig_continue;
	struct _colors colors[NUM_LED],prev_colors[NUM_LED];
	int progress_bar_percent;
	xSemaphoreHandle _sem;
	xSemaphoreHandle _animate_sem;
}self;

static bool lock() {
	return xSemaphoreTake(self._sem, portMAX_DELAY) == pdPASS;
}
static void unlock() {
	xSemaphoreGive(self._sem);
}

static bool _start_animation( unsigned int timeout ) {
	if( lock_animation( timeout ) ) {
		lock();
		self.counter = 0;
		self.sig_continue = true; //careful, to set this true requires both semaphores
		unlock();
		return true;
	}
	return false;
}

static bool _animate_pulse(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	bool sig_continue;
	lock();
	self.counter++;
	out_r[self.counter%rgb_array_size] = 60;
	out_g[self.counter%rgb_array_size] = 60;
	out_b[self.counter%rgb_array_size] = 60;
	*out_delay = 20;
	sig_continue = self.sig_continue;
	unlock();
	return sig_continue;
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
static bool _animate_trippy(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	bool sig_continue;
	lock();
	for(i = 0; i < NUM_LED; i++){
		if(_reach_color(&self.prev_colors[i].r, self.colors[i].r)){
			self.colors[i].r = rand()%32 + (self.counter++)%32;
		}
		if(_reach_color(&self.prev_colors[i].g, self.colors[i].g)){
			self.colors[i].g = rand()%32 + (self.counter++)%32;
		}
		if(_reach_color(&self.prev_colors[i].b, self.colors[i].b)){
			self.colors[i].b = rand()%32 + (self.counter++)%32;
		}
		out_r[i] = self.prev_colors[i].r;
		out_g[i] = self.prev_colors[i].g;
		out_b[i] = self.prev_colors[i].b;
	}
	*out_delay = 20;

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
	*out_delay = 20;
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
	*out_delay = 500;
	sig_continue = self.sig_continue;
	unlock();
	return sig_continue;
}

/*
 * Pubic:
 */

void init_led_animation() {
	vSemaphoreCreateBinary(self._sem);
	vSemaphoreCreateBinary(self._animate_sem);
}
bool lock_animation( unsigned int timeout ) {
	bool success;
	lock();
	success = xSemaphoreTake(self._animate_sem, timeout) == pdPASS;
	unlock();
	return success;
}
void unlock_animation() {
	lock();
	xSemaphoreGive(self._animate_sem);
	unlock();
}

bool play_led_animation_pulse(unsigned int timeout){
	if( _start_animation( timeout ) ) {
		led_start_custom_animation(_animate_pulse, NULL);
		return true;
	}
	return false;
}
bool play_led_trippy(unsigned int timeout){
	int i;
	if( _start_animation( timeout ) ) {
		for(i = 0; i < NUM_LED; i++){
			self.colors[i] = (struct _colors){rand()%120, rand()%120, rand()%120};
			self.prev_colors[i] = (struct _colors){0};
		}
		led_start_custom_animation(_animate_trippy, NULL);
		return true;
	}
	return false;
}
bool play_led_progress_bar(int r, int g, int b, unsigned int options, unsigned int timeout){
	if( _start_animation( timeout ) ) {
		self.colors[0] = (struct _colors){r, g, b};
		self.progress_bar_percent = 0;
		led_start_custom_animation(_animate_progress, NULL);
		return true;
	}
	return false;
}
void set_led_progress_bar(uint8_t percent){
	lock();
	self.progress_bar_percent =  percent > 100?100:percent;
	unlock();
}
void stop_led_animation(void){
	lock();
	self.sig_continue = false;
	unlock();
	unlock_animation();
}
int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			lock();
			self.sig_continue = false;
			unlock();
			return 0;
		}else if(strcmp(argv[1], "+") == 0){
			set_led_progress_bar(self.progress_bar_percent += 5);
			return 0;
		}
		else if(strcmp(argv[1], "-") == 0){
			set_led_progress_bar(self.progress_bar_percent -= 5);
			return 0;
		}
	}
	//play_led_trippy( portMAX_DELAY );
	play_led_progress_bar(20, 20, 20, 0, portMAX_DELAY);
	return 0;
}
bool factory_led_test_pattern(unsigned int timeout) {
	if( _start_animation( timeout ) ) {
		led_start_custom_animation(_animate_factory_test_pattern, NULL);
		return true;
	}
	return false;
}

