#include "gesture.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "uartstdio.h"
#include "FreeRTOS.h"

/* defines the frames per second of the gesture fsm, which is triggerd by sampling the prox*/
#define GESTURE_FPS 10

/* how much delta does it take to activate the fsm over noise floor */
#define DETECTION_THRESH 100

/* multiples GESTURE FSP, minimal frames require for the wave gesture */
#define GESTURE_WAVE_MULTIPLIER (0.2)

/* multiples GESTURE FSP, minimal frames require for the hold gesture */
#define GESTURE_HOLD_MULTIPLIER (1)


static struct{
	struct{
		enum fsm_state{
			GFSM_IDLE = 0,
			GFSM_LEVEL,
			GFSM_HOLD,
			GFSM_SLIDE,
		}state;

		int exceed_thresh_count;
		int prox_impluse;
		int prox_slow;
		int prox_last;
	}fsm;
	gesture_callbacks_t user;
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
static int _fsm(int in){
	int exceeded = 0;
	//computes the average of last 3 frames of energy
	//UARTprintf("%d\r\n", in);

	if( in > DETECTION_THRESH ) {
		exceeded = 1;
	}

	switch(self.fsm.state){
	case GFSM_IDLE:
		//any edge triggers edge up state
		if( exceeded > 0 ){
			UARTprintf("->1\r\n");
			_transition_state(GFSM_LEVEL);
		}
		break;
	case GFSM_LEVEL:
		if (!exceeded || _hasHold() ) {
			if (_hasHold()) {
				UARTprintf("Gesture: HOLD\r\n");
				if (self.user.on_hold) {
					self.user.on_hold(self.user.ctx);
				}
				_transition_state(GFSM_HOLD);
			} else if (_hasWave()) {
				UARTprintf("->0 ");
				if (_hasWave()) {
					UARTprintf("Gesture: WAVE\r\n");
					if (self.user.on_wave) {
						self.user.on_wave(self.user.ctx);
					}
				}
				_transition_state(GFSM_IDLE);
				UARTprintf("\r\n");
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
	return 0;
}

static int _fsm_reset(void){
	self.fsm.state = GFSM_IDLE;
	self.fsm.exceed_thresh_count = 0;
	self.fsm.prox_impluse = 0;
	self.fsm.prox_slow = 0;
	self.fsm.prox_last =0;
	return 0;
}
void gesture_init(gesture_callbacks_t * _user){
	_fsm_reset();
	if(_user){
		self.user = *_user;
	}
}

int disp_prox;
void gesture_input(int prox, int light){


	if (self.fsm.prox_last != 0) {
		self.fsm.prox_slow += (prox - self.fsm.prox_slow) / 64;
		if( (prox-self.fsm.prox_slow) > 0 ) {
			self.fsm.prox_slow = prox;
		}
		prox -= self.fsm.prox_slow;
		self.fsm.prox_impluse  = abs( prox );

		if( disp_prox ) {
			UARTprintf( "%d\t", self.fsm.prox_impluse );
		}
		_fsm(self.fsm.prox_impluse);
	} else {
		self.fsm.prox_slow = prox;
	}
	self.fsm.prox_last = prox;
}
