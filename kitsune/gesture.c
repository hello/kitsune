#include "gesture.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "uartstdio.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stdbool.h"

/* defines the frames per second of the gesture fsm, which is triggerd by sampling the prox*/
#define GESTURE_FPS 10

/* how much delta does it take to activate the fsm over noise floor */
#define DETECTION_THRESH 100

/* multiples GESTURE FSP, minimal frames require for the wave gesture */
#define GESTURE_WAVE_MULTIPLIER (0.1)

/* multiples GESTURE FSP, minimal frames require for the hold gesture */
#define GESTURE_HOLD_MULTIPLIER (1)


static struct{
	struct{
		enum fsm_state{
			GFSM_IDLE = 0,
			GFSM_IDLE_FORREALS,
			GFSM_LEVEL,
			GFSM_HOLD,
			GFSM_SLIDE,
		}state;

		int exceed_thresh_count;
		int prox_impluse;
		int prox_slow;
		int prox_last;
	}fsm;
	int wave_count;
	int hold_count;
	xSemaphoreHandle gesture_count_semaphore;
}self;

static bool _hasWave(void){
	return (self.fsm.exceed_thresh_count > GESTURE_FPS * GESTURE_WAVE_MULTIPLIER);
}
static bool _hasHold(void){
	return (self.fsm.exceed_thresh_count > GESTURE_FPS * GESTURE_HOLD_MULTIPLIER);
}

static void _transition_state(enum fsm_state s){
	self.fsm.exceed_thresh_count = 0;
	self.fsm.state = s;
}

static int _fsm_reset(void){
	self.fsm.state = GFSM_IDLE;
	self.fsm.exceed_thresh_count = 0;
	self.fsm.prox_impluse = 0;
	self.fsm.prox_slow = 0;
	self.fsm.prox_last =0;
	return 0;
}
static gesture _fsm(int in){
	gesture ret = GESTURE_NONE;
	int exceeded = 0;
	//computes the average of last 3 frames of energy
	//LOGI("%d\r\n", in);

	if( in > DETECTION_THRESH ) {
		exceeded = 1;
	}

	switch(self.fsm.state){
	case GFSM_IDLE:
		//any edge triggers edge up state
		LOGI("->0\r\n");
		_transition_state(GFSM_IDLE_FORREALS);
		//no break
	case GFSM_IDLE_FORREALS:
		//any edge triggers edge up state
		if( exceeded > 0 ){
			LOGI("->1\r\n");
			_transition_state(GFSM_LEVEL);
		}
		break;
	case GFSM_LEVEL:
		if (!exceeded || _hasHold() ) {
			if (_hasHold()) {
				LOGI("Gesture: HOLD\r\n");
				if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
				{
					self.hold_count += 1;
					xSemaphoreGive(self.gesture_count_semaphore);
				}
				_transition_state(GFSM_HOLD);
				ret = GESTURE_HOLD;
			} else if (_hasWave()) {
				if (_hasWave()) {
					LOGI("Gesture: WAVE\r\n");
					if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
					{
						self.wave_count += 1;
						xSemaphoreGive(self.gesture_count_semaphore);
					}
					ret = GESTURE_WAVE;
				}
				_transition_state(GFSM_IDLE);
				LOGI("\r\n");
			} else {
				_transition_state(GFSM_IDLE);
			}
		}
		break;
	case GFSM_HOLD:
		if(!exceeded){
			_transition_state(GFSM_IDLE);
		}
		break;
	}

	if( exceeded ) {
		++self.fsm.exceed_thresh_count;
	} else {
		self.fsm.exceed_thresh_count = 0;
	}
	return ret;
}

void gesture_init(){
	_fsm_reset();
	self.gesture_count_semaphore = xSemaphoreCreateMutex();
}

int disp_prox;
gesture gesture_input(int prox){
	if( disp_prox ) {
		LOGI( "%d %d\t", prox, self.fsm.prox_impluse );
	}

	gesture result = GESTURE_NONE;
	if (self.fsm.prox_last != 0) {
		self.fsm.prox_slow += (prox - self.fsm.prox_slow) / 32;
		if( (prox-self.fsm.prox_slow) > 0 ) {
			self.fsm.prox_slow = prox;
		}
		prox -= self.fsm.prox_slow;
		self.fsm.prox_impluse  = abs( prox );
		result = _fsm(self.fsm.prox_impluse);
	} else {
		self.fsm.prox_slow = prox;
	}
	self.fsm.prox_last = prox;
	return result;
}

int gesture_get_wave_count()
{
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		int ret = self.wave_count;
		xSemaphoreGive(self.gesture_count_semaphore);

		return ret;
	}

	return 0;
}

int gesture_get_hold_count()
{
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		int ret = self.hold_count;
		xSemaphoreGive(self.gesture_count_semaphore);
		return ret;
	}

	return 0;
}

void gesture_counter_reset()
{
	if(xSemaphoreTake(self.gesture_count_semaphore, 100) == pdTRUE)
	{
		self.hold_count = 0;
		self.wave_count = 0;
		xSemaphoreGive(self.gesture_count_semaphore);
	}
}
