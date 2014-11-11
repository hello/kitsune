#include "gesture.h"
#include <string.h>
#include "FreeRTOS.h"
#define GESTURE_FPS 10
#define NOISE_SAMPLES (GESTURE_FPS * 2)
#define NORM_FLOOR_SAMPLES (GESTURE_FPS * 2)
#define GESTURE_RESOLUTION 10000

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
			GFSM_FIN_HOLD
		}state;
		_fifo_t * frame;
		int total_energy;
		int frame_count;;
		int prev_in;
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
#define FSM_ENERGY_DECAY 1
static int _fsm(int in, int th){
	if( 0 != _putfifo(self.fsm.frame,in)){
		return 0;
	}
	//UARTprintf("> %d / %d >", in, th);
	int average_energy = _avgfifo(self.fsm.frame);
	switch(self.fsm.state){
	case GFSM_IDLE:
		//any edge triggers edge up state
		if(in > (th * 3)){
			UARTprintf("->1");
			self.fsm.state = GFSM_LEVEL;
		}
		break;
	case GFSM_LEVEL:
		self.fsm.total_energy += abs(in - self.fsm.prev_in);
		self.fsm.frame_count++;
		_putfifo(self.fsm.frame,in);
		if(average_energy < 1){
			UARTprintf("->0");

			if(self.fsm.total_energy > 1000 && self.fsm.frame_count > (GESTURE_FPS * 1)){
				UARTprintf(": HOLD");
				self.fsm.state = GFSM_FIN_HOLD; //so we dont trigger hold multiple times
			}else if(self.fsm.total_energy > 10 && self.fsm.frame_count >= 3){
				UARTprintf(": WAVE");
			}
	//duff's device (http://en.wikipedia.org/wiki/Duff's_device)
	case GFSM_FIN_HOLD:
			self.fsm.frame_count = 0;
			self.fsm.total_energy = 0;
			self.fsm.state = GFSM_IDLE;
		}
		break;
	}
	self.fsm.prev_in = in;
	//UARTprintf("\r\n");
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
			*out = (env - in);// * GESTURE_RESOLUTION / env;
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
