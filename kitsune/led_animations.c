#include "led_cmd.h"
#include <hw_memmap.h>
#include <stdlib.h>
#include "rom_map.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include <string.h>
static struct{
	int counter;
	bool sig_stop;
}self;
static bool _animate_pulse(int * out_r, int * out_g, int * out_b, int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	self.counter++;
	out_r[self.counter%rgb_array_size] = 60;
	out_g[self.counter%rgb_array_size] = 60;
	out_b[self.counter%rgb_array_size] = 60;
	*out_delay = 20;
	return self.sig_stop;
}




/*
 * Pubic:
 */
void play_led_animation_pulse(void){
	self.counter = 0;
	self.sig_stop = true;
	led_start_custom_animation(_animate_pulse, NULL);
}
void stop_led_animation(void){
	self.sig_stop = false;
}
int Cmd_led_animate(int argc, char *argv[]){
	//demo
	if(argc > 1){
		if(strcmp(argv[1], "stop") == 0){
			self.sig_stop = false;
			return 0;
		}
	}
	play_led_animation_pulse();
	return 0;
}
