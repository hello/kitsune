#include "prox_signal.h"
#include <string.h>
#include "hellomath.h"
#include "uart_logger.h"
#include <stdlib.h>
#include "hellomath.h"
#include "FreeRTOS.h"
#include "task.h"

/***********************
 * STATIC DATA
 */

static int32_t fast=0;

typedef enum {
	HELD,
	INCOMING,
	OUTGOING,
	IDLE,
} prox_state_t;
static prox_state_t state=IDLE;
static uint32_t mark=0;

#define ACC_SZ 5
static uint32_t diff_acc_buf[ACC_SZ]={0};
static uint32_t diff_acc_idx=0;

void ProxSignal_Init(void) {
	fast = 0;
}

ProxGesture_t ProxSignal_UpdateChangeSignals( int32_t new) {
#define SLOWDOWN 4
	ProxGesture_t gesture = proxGestureNone;
	int32_t diff, diff_acc;
	int i;

	new <<= SLOWDOWN;
	if( fast == 0 ) {
		fast = new;
	}
	fast += (new-fast)>>SLOWDOWN;

	diff = fast - new;
	diff>>=SLOWDOWN;

	diff_acc_buf[++diff_acc_idx%ACC_SZ] = diff;
	diff_acc = 0;
	for(i=0;i<ACC_SZ;++i) {
		diff_acc += diff_acc_buf[i];
	}

	if( diff_acc > 50 ) {
		switch( state ) {
		case OUTGOING:
		case IDLE:
		default:
			mark = xTaskGetTickCount();
			LOGP("%d->%d mark %d\n", state, INCOMING, mark);
			//break; deliberate fallthrough
		case INCOMING:
			gesture = proxGestureIncoming;
			state = INCOMING;
			if( diff_acc > 1500 && xTaskGetTickCount() - mark > 3000 ) {
				LOGP("held\n");
				state = HELD;
				gesture = proxGestureHold;
				fast = new;
			}
			break;
		case HELD:
			break; //do nothing...
		}
	} else if( diff_acc < -50 ) {
		switch( state ) {
		case INCOMING:
			LOGP("%d->%d wave\n", state, OUTGOING);
			gesture = proxGestureWave;
			state = OUTGOING;
			LOGP("\n");
			break;
		case HELD:
			if( diff_acc < -5000 ) {
				LOGP("%d->%d release\n", state, OUTGOING);
				gesture = proxGestureRelease;
				state = OUTGOING;
				fast = new;
			}
			break;
		case IDLE:
		case OUTGOING:
		default:
			state = OUTGOING;
			break;
		}
	}
	if( gesture != proxGestureHold && state == HELD ) {
		gesture = proxGestureHeld;
		//check if we're stuck in hold...
		if( xTaskGetTickCount() - mark > 60000 ) {
			gesture = proxGestureRelease;
			state = IDLE;
		}
	}

	LOGP( "%d\t%d\t%d\t%d\t%d\n", new, state, gesture, diff_acc, diff );

	return gesture;
}
