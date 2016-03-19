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

#define DIFF_1SEC 20
static int diff_1sec_buf[DIFF_1SEC]={0};
static int ctr=0;
#define DIFF_QTR_SEC 5
static int diff_qtr_buf[DIFF_QTR_SEC]={0};

void ProxSignal_Init(void) {
	fast = 0;
}

static void calc_buffer( int in, int * buffer, int ctr, int sz, int * mean, int * var) {
	int i;
	buffer[ctr%sz] = in;
	*mean = 0;
	for(i=0;i<sz;++i) {
		*mean += buffer[i];
	}
	*mean /=sz;
	for(i=0;i<sz;++i) {
		*var += ( buffer[i] - *mean )*( buffer[i] - *mean );
	}
	*var /=sz;
}

ProxGesture_t ProxSignal_UpdateChangeSignals( int32_t new) {
#define SLOWDOWN 4
	ProxGesture_t gesture = proxGestureNone;
	int diff, diff_1sec_mean=0, diff_1sec_var=0, diff_qtr_mean=0, diff_qtr_var=0;

	new <<= SLOWDOWN;
	if( fast == 0 ) {
		fast = new;
	}
	fast += (new-fast)>>SLOWDOWN;

	diff = fast - new;
	diff>>=SLOWDOWN;

	calc_buffer( diff, diff_1sec_buf, ++ctr, DIFF_1SEC, &diff_1sec_mean, &diff_1sec_var);
	calc_buffer( diff, diff_qtr_buf, ctr, DIFF_QTR_SEC, &diff_qtr_mean, &diff_qtr_var);

	if( diff_1sec_var < 80 ) {
		state = IDLE;
	}

	if( diff_qtr_mean > 70 ) {
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

			LOGP("%d %d\n", diff_qtr_mean, xTaskGetTickCount() - mark);
			if( diff_qtr_mean > 1500 && xTaskGetTickCount() - mark > 1000 ) {
				LOGP("held\n");
				state = HELD;
				gesture = proxGestureHold;
				fast = new;
			}
			break;
		case HELD:
			break; //do nothing...
		}
	} else if( diff_qtr_mean < -60 ) {
		switch( state ) {
		case IDLE:
		case INCOMING:
			LOGP("%d->%d wave\n", state, OUTGOING);
			gesture = proxGestureWave;
			state = OUTGOING;
			LOGP("\n");
			break;
		case HELD:
			if( diff_qtr_mean < -5000 ) {
				LOGP("%d->%d release\n", state, OUTGOING);
				gesture = proxGestureRelease;
				state = OUTGOING;
				fast = new;
			}
			break;
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

	LOGP("%d\t%d\t%d\t%d\t%d\t%d\t%d\n", state, gesture, diff_1sec_mean, diff_1sec_var,
			diff_qtr_mean, diff_qtr_var, diff);

	return gesture;
}
