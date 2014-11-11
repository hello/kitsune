#include "gesture.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "uartstdio.h"
#include "FreeRTOS.h"
#define GESTURE_FPS 10
#define GESTURE_ACTIVATION_MULTIPLIER 5

#define GESTURE_WAVE_MULTIPLIER (0.3)
#define GESTURE_WAVE_ENERGY_TH	50

#define GESTURE_HOLD_MULTIPLIER (1)
#define GESTURE_WAVE_ENERGY_TH 1000


#define NOISE_SAMPLES (GESTURE_FPS * 2)
#define NORM_FLOOR_SAMPLES (GESTURE_FPS * 2)

typedef struct{
	int idx;
	int size;
	int count;
	int bins[0];
}_fifo_t;

static struct{
	struct{
		_fifo_t * avg;
		int prev;
	}mavg; //for noise
	struct{
		_fifo_t * env;
	}normalizer; //for input

	struct{
		enum fsm_state{
			GFSM_IDLE = 0,
			GFSM_LEVEL,
		}state;
		_fifo_t * frame;
		int total_energy;
		int frame_count;
		int prev_in;
		bool held;
	}fsm;
	gesture_callbacks_t user;
}self;
static _fifo_t * _mkfifo(int size){
	_fifo_t * ret = (_fifo_t*)pvPortMalloc(sizeof(_fifo_t) + size * sizeof(int));
	if(ret){
		ret->idx = 0;
		ret->size = size;
		ret->count = 0;
		memset(ret->bins, 0, size);
	}
	return ret;
}
static int _putfifo(_fifo_t * f, int n){
	f->bins[f->idx] = n;
	f->idx = (f->idx + 1) % f->size;
	if(f->count++ > f->size){
		return 0;
	}
	return -1;
}
static int _avgfifo(_fifo_t * f){
	int i, sum = 0;
	for(i = 0; i < f->size; i++){
		sum += f->bins[i];
	}
	return sum/f->size;
}
static int _mavg_reset(void){
	if(self.mavg.avg){
		vPortFree(self.mavg.avg);
	}
	self.mavg.avg = _mkfifo(NOISE_SAMPLES);
	self.mavg.prev = 0;
	if(self.mavg.avg){
		return 0;
	}
	return -1;
}
static int _normalizer_reset(void){
	if(self.normalizer.env){
		vPortFree(self.normalizer.env);
	}
	self.normalizer.env = _mkfifo(NORM_FLOOR_SAMPLES);
	if(self.normalizer.env){
		return 0;
	}
	return -1;
}
static int _fsm_reset(void){
	self.fsm.state = GFSM_IDLE;
	self.fsm.total_energy = 0;
	self.fsm.frame_count = 0;
	if(self.fsm.frame){
		vPortFree(self.fsm.frame);
	}
	self.fsm.frame = _mkfifo(3);
	return 0;
}

static bool _hasWave(void){
	return (self.fsm.total_energy > GESTURE_WAVE_ENERGY_TH && self.fsm.frame_count >= GESTURE_FPS * GESTURE_WAVE_MULTIPLIER);
}
static bool _hasHold(void){
	return (self.fsm.total_energy > GESTURE_HOLD_ENERGY_TH && self.fsm.frame_count >= GESTURE_FPS * GESTURE_HOLD_MULTIPLIER);
}
static int _fsm(int in, int th){
	if( 0 != _putfifo(self.fsm.frame,in)){
		return 0;
	}
	//computes the average of last 3 frames of energy
	int average_energy = _avgfifo(self.fsm.frame);
	switch(self.fsm.state){
	case GFSM_IDLE:
		//any edge triggers edge up state
		if(in > (th * GESTURE_ACTIVATION_MULTIPLIER)){
			UARTprintf("> %d / %d", in, th);
			UARTprintf("->1\r\n");
			self.fsm.state = GFSM_LEVEL;
			self.fsm.held = false;
		}
		break;
	case GFSM_LEVEL:
		//use total energy and frame count to determine wave type
		self.fsm.total_energy += abs(in - self.fsm.prev_in);
		self.fsm.frame_count++;
		if(average_energy < 1){
			UARTprintf("->0 ");
			if(!self.fsm.held && _hasWave()){
				UARTprintf("Gesture: WAVE\r\n");
				if(self.user){
					self.user.on_wave(0,0);
				}
			}
			self.fsm.frame_count = 0;
			self.fsm.total_energy = 0;
			self.fsm.state = GFSM_IDLE;
			UARTprintf("\r\n");
		}else if(!self.fsm.held && _hasHold()){
			UARTprintf("Gesture: HOLD\r\n");
			self.fsm.held = true;
			if(self.user){
				self.user.on_hold(0, 0);
			}
		}
		break;
	}
	self.fsm.prev_in = in;
	return 0;
}
static int _normalize(int in, int * out){
	int ret = -1;
	int env;
	if(in >= 200000){
		//clamping is necessary to prevent overflow
		in = 200000;
	}
	ret = _putfifo(self.normalizer.env, in);
	if(ret == 0){
		env = _avgfifo(self.normalizer.env);
		if(in < env){
			*out = (env - in);
		}else{
			*out = 0;
		}
		ret = 0;
	}
	return ret;
}
static int _mavg(int in, int * out){
	int delta = abs(in - self.mavg.prev);
	int ret = _putfifo(self.mavg.avg, delta);
	if(ret == 0){
		*out = _avgfifo(self.mavg.avg);
	}
	self.mavg.prev = in;
	return ret;
}

void gesture_init(gesture_callbacks_t * _user){
	_mavg_reset();
	_normalizer_reset();
	_fsm_reset();
	if(_user){
		self.user = *_user;
	}
}
void gesture_input(int prox, int light){
	int output, noise_th;
	int a,b;
	a = _mavg(prox, &noise_th);
	b = _normalize(prox, &output);
	if(!a && !b){
		_fsm(output, noise_th);
	}

}
