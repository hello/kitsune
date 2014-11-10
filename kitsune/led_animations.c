#include "led_cmd.h"
#include <hw_memmap.h>
#include <stdlib.h>
#include "rom_map.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include <string.h>
#include <stdlib.h>
struct _colors{
		int r,g,b;
};
static struct{
	int counter;
	volatile bool sig_continue;
	struct _colors colors[NUM_LED],prev_colors[NUM_LED];
	int progress_bar_percent;
}self;


static bool _animate_pulse(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	self.counter++;
	out_r[self.counter%rgb_array_size] = 60;
	out_g[self.counter%rgb_array_size] = 60;
	out_b[self.counter%rgb_array_size] = 60;
	*out_delay = 20;
	return self.sig_continue;
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
	return self.sig_continue;
}
static bool _animate_progress(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	int filled = self.progress_bar_percent * rgb_array_size / 100;
	int i;
	for(i = 0; i < filled && i < rgb_array_size; i++){
		out_r[i] = self.colors[0].r;
		out_g[i] = self.colors[0].g;
		out_b[i] = self.colors[0].b;
	}
	if(filled < rgb_array_size && self.counter%2 == 0){
		out_r[filled] = self.colors[0].r;
		out_g[filled] = self.colors[0].g;
		out_b[filled] = self.colors[0].b;
	}
	self.counter++;
	*out_delay = 200;
	return self.sig_continue;
}

/*
 * Pubic:
 */
void play_led_animation_pulse(void){
	self.counter = 0;
	self.sig_continue = true;
	led_start_custom_animation(_animate_pulse, NULL);
}
void play_led_trippy(void){
	int i;
	for(i = 0; i < NUM_LED; i++){
		self.colors[i] = (struct _colors){rand()%120, rand()%120, rand()%120};
		self.prev_colors[i] = (struct _colors){0};
	}
	self.counter = 0;
	self.sig_continue = true;
	led_start_custom_animation(_animate_trippy, NULL);
}
void play_led_progress_bar(int r, int g, int b, unsigned int options){
	self.colors[0] = (struct _colors){r, g, b};
	self.sig_continue = true;
	self.progress_bar_percent = 0;
	led_start_custom_animation(_animate_progress,NULL);
}
void set_led_progress_bar(uint8_t percent){
	self.progress_bar_percent =  percent > 100?100:percent;
}
void stop_led_animation(void){
	self.sig_continue = false;
}
int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			self.sig_continue = false;
			return 0;
		}else if(strcmp(argv[1], "+") == 0){
			set_led_progress_bar(self.progress_bar_percent += 10);
			return 0;
		}
		else if(strcmp(argv[1], "-") == 0){
			set_led_progress_bar(self.progress_bar_percent -= 10);
			return 0;
		}
	}
	play_led_progress_bar(20, 20, 20, 0);
	return 0;
}
