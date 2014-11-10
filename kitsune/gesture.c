#include "gesture.h"
#include <string.h>
#include "FreeRTOS.h"
#define GESTURE_FPS 10
#define FIFO_SAMPLES 3
#define NORM_FLOOR_SAMPLES (GESTURE_FPS * 3)
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
	}mavg;
	struct{
		_fifo_t * env;
	}normalizer;

	struct{
		enum fsm_state{
			GFSM_IDLE = 0,
			GFSM_START,
		}state;
	}fsm_data;
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
	self.mavg.avg = _mkfifo(FIFO_SAMPLES);
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
	self.fsm_data.state = GFSM_IDLE;
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
			*out = (env - in) * GESTURE_RESOLUTION / env;
		}else{
			*out = 0;
		}
		ret = 0;
	}
	return ret;
}
static int _mavg(int in, int * out){
	int ret = _putfifo(self.mavg.avg, in);
	if(ret == 0){
		*out = _avgfifo(self.mavg.avg);
	}
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
	int output;
	if(0 != _mavg(prox, &output)){
		return;
	}
	if(0 != _normalize(output, &output)){
		return;
	}
	UARTprintf("normal %d\r\n", output);
}
