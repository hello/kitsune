#include "prox_signal.h"
#include <string.h>
#include "hellomath.h"
#include "uart_logger.h"
#include <stdlib.h>
#include "hellomath.h"
#include "FreeRTOS.h"
#include "task.h"
/************************
 * DEFINES AND TYPEDEFS
 */
#define CHANGE_SIGNAL_BUF_SIZE_2N (3)
#define CHANGE_SIGNAL_BUF_SIZE (1 << CHANGE_SIGNAL_BUF_SIZE_2N)
#define CHANGE_SIGNAL_BUF_MASK (CHANGE_SIGNAL_BUF_SIZE - 1)

#define STABLE_BUF_SIZE_2N (3)
#define STABLE_BUF_SIZE (1 << STABLE_BUF_SIZE_2N)
#define STABLE_BUF_MASK (STABLE_BUF_SIZE - 1)

#define QFIXEDPOINT (10)

#define PROX_SIGNAL_SAMPLE_PERIOD_MS (100)

#define PROX_HOLD_GESTURE_PERIOD_MS (1000)

#define PROX_HOLD_COUNT_THRESHOLD (PROX_HOLD_GESTURE_PERIOD_MS / PROX_SIGNAL_SAMPLE_PERIOD_MS)

#define MAX_COUNTER (32767)

#define DIFF_SIGNAL_THRESHOLD_FOR_INTERRUPTION (50)

/*
#define PROX_SIGNAL_BUF_SIZE_2N (3)
#define PROX_SIGNAL_BUF_SIZE (1 << CHANGE_SIGNAL_BUF_SIZE_2N)
#define PROX_SIGNAL_BUF_MASK (CHANGE_SIGNAL_BUF_SIZE - 1)
*/

typedef enum {
	stable,
	increasing,
	decreasing,
	numChangeModes
} EChangeModes_t;

typedef struct {
	int64_t accumulator;
	int32_t changebuf[CHANGE_SIGNAL_BUF_SIZE];
	int32_t stablebuf[STABLE_BUF_SIZE];
	uint32_t stablebufcount;
	int32_t maxstable;

	int32_t logProbOfModes[numChangeModes];
	uint32_t counter;
	uint32_t stablecount;
	int32_t minHeldStable;
	uint32_t increasingCount;
	uint32_t decreasingCount;
	uint32_t holdStart;
	uint8_t isHeld;
	uint8_t isInterrupted;
} ProxSignal_t;


/***********************
 * STATIC DATA
 */

static ProxSignal_t _data;
//the higher this gets, the less likely you are to be stable
static const int32_t k_stable_likelihood_coefficient = TOFIX(0.9f,QFIXEDPOINT); //todo play with this

//the closer this gets to zero, the more likely it is that you will be increasing or decreasing
static const int32_t k_change_log_likelihood = TOFIX(-0.01f,QFIXEDPOINT);

//the closer this gets to zero, the shorter the amount of time it will take to switch between modes
//the more negative it gets, the more evidence is required before switching modes, in general
static const int32_t k_min_log_prob = TOFIX(-0.10f,QFIXEDPOINT);

static const int32_t k_hold_threshold = 1000; //difference between max and held stable value before we say we are holding
static const int32_t k_release_threshold = 1000; //difference between min and held stable value before we say we are released

void ProxSignal_Init(void) {
	memset(&_data,0,sizeof(_data));
}

static void UpdateMaxInBuffer(int32_t x) {
	const uint32_t idx = _data.stablebufcount & STABLE_BUF_MASK;
	int16_t i;
	int32_t max = MIN_INT_32;
	const int32_t * cbuf = _data.stablebuf;

	_data.stablebuf[idx] = x;
	for (i = 0; i < STABLE_BUF_SIZE; i++) {
		if (cbuf[i] > max) {
			max = cbuf[i];
		}
	}

	//LOGI("new max %d, %d\n",idx,max);

	_data.maxstable = max;
	_data.stablebufcount++;


}

static ProxGesture_t GetGesture(uint8_t isInterrupted, EChangeModes_t mode,uint8_t hasStableMeas,const int32_t maxStable,const int32_t stablex,const int32_t currentx) {

	if (hasStableMeas) {
		const int32_t diff = maxStable - stablex;

		//LOGI("hold %d stb %d\n", diff, _data.stablecount);
		if (diff > k_hold_threshold) {
			if (_data.stablecount > PROX_HOLD_COUNT_THRESHOLD) {
				//output one hold
				if (!_data.isHeld) {
					LOGI("ProxSignal: HOLDING\n");
					_data.holdStart = xTaskGetTickCount();
					_data.isHeld = 1;
					_data.minHeldStable = stablex;
					return proxGestureHold;
				}
			}

		}
	}

	//if you are held, then evaluate if you should not be held
	if (_data.isHeld) {
		const int32_t diff = currentx - _data.minHeldStable;
		//LOGI("current=%d,min=%d,diff=%d\n",currentx,_data.minHeldStable,diff);
		if (diff > k_release_threshold || xTaskGetTickCount() - _data.holdStart > MAX_HOLD_TIME_MS) {
			_data.isHeld = 0;
			LOGI("ProxSignal: RELEASE\n");
			return proxGestureRelease;
		}
	}

	if (!_data.isHeld) {
		if (isInterrupted && !_data.isInterrupted) {
			_data.isInterrupted = 1;
			return proxGestureWave;
		}
		else if (!isInterrupted && _data.isInterrupted) {
			_data.isInterrupted = 0;
		}
	}
	else {
		_data.isInterrupted = 0;
	}




	return proxGestureNone;

}

ProxGesture_t ProxSignal_UpdateChangeSignals(const int32_t newx) {
	const uint16_t idx = _data.counter & CHANGE_SIGNAL_BUF_MASK;
	const int32_t oldx = _data.changebuf[idx];
	EChangeModes_t mode = stable;
	int32_t diffsignal;
	int32_t change32;
	int16_t change16;
	int16_t logLikelihoodOfModePdfs[numChangeModes];
	int32_t * logProbOfModes = _data.logProbOfModes;
	int16_t i;
	int16_t maxidx;
	int32_t maxlogprob;
	uint8_t hasStableMeas = 0;
	int32_t stablex = 0;
	uint8_t isInterrupted = 0;

	/* increment counter */
	_data.counter++;

	/*  process signal */

	/* update buffer */
	_data.changebuf[idx] = newx;

	change32 = (int32_t)newx - (int32_t)oldx;

	if (change32 < MIN_INT_16) {
		change32 = MIN_INT_16;
	}

	if (change32 > MAX_INT_16) {
		change32 = MAX_INT_16;
	}

	change16 = change32;
	//LOGI("change %d\n", change16 );

	//evaluate log likelihood and compute Bayes update
	/*


     This is my likelihood function
     exp(-|x|/a)   I could do exp(-x^2 / b) or something, but I don't want to deal with the x^2 in fixed point

     Hypothesis 1 - No change

               p(x)
            /|\
           / | \
          /  |  \
         /   |   \
    <--------0----------->  change signal (x)


     Hypothesis 2 - change

     uniform likelihood function

             |
             |
             |   p(x)
    <--------|--------------->
    <--------0----------->   change signal


	 */

	if (change16 > 0) {
		logLikelihoodOfModePdfs[increasing] = k_change_log_likelihood;
		logLikelihoodOfModePdfs[decreasing] = MIN_INT_16;
	}
	else if (change16 < 0) {
		logLikelihoodOfModePdfs[increasing] = MIN_INT_16;
		logLikelihoodOfModePdfs[decreasing] = k_change_log_likelihood;

	}
	else {
		logLikelihoodOfModePdfs[increasing] = k_change_log_likelihood;
		logLikelihoodOfModePdfs[decreasing] = k_change_log_likelihood;
	}

	change16 = -abs(change16);
	logLikelihoodOfModePdfs[stable] = MUL_PRECISE_RESULT(change16,k_stable_likelihood_coefficient,QFIXEDPOINT);
	/*LOGI("loglik %d %d %d\n", logLikelihoodOfModePdfs[increasing],
			logLikelihoodOfModePdfs[stable],
			logLikelihoodOfModePdfs[decreasing]);*/


	/* Bayes rule

                    P(B|Ai) P(Ai)
       P(Ai|B) =   ------------------
                   sum_j(P(B|Aj) P(Aj))


       or for a continuous measurements

                    P(y | Ai) P(Ai)
       P(Ai|y) =  -----------------
                   sum_j[P(x|Aj) P(Aj)]



        Of course the denominator is just there to make sure that sum(P) = 1, so let's just ignore that
        for now and take the log of everything

        log P(Ai | y) = log P(y | Ai) + log P(Ai)

      So for our two hypotheses:

         log P(change | y ) = log P(y | change) + log P(change)

         log P(no change | y) = log P(y | no change) + log P(no change)

	 *** updating log likelihood is now just an addition ***

      Which hypothesis is more likely?  loglik1 > loglik2? hypothesis 1 is more likely
      Since we're just tracking the differnece between logP1 and logP2, if this difference
      (let's call it "z") exceeds 0, then P2 is more likely than P1

	 */


	for (i = 0; i < numChangeModes; i++) {
		logProbOfModes[i] += logLikelihoodOfModePdfs[i];
	}


	/* normalize -- the highest log probability will now be 0 */
	maxlogprob = logProbOfModes[0];
	maxidx = 0;
	for (i = 1; i < numChangeModes; i++) {
		if (logProbOfModes[i] > maxlogprob) {
			maxlogprob = logProbOfModes[i];
			maxidx = i;
		}
	}

	for (i = 0; i < numChangeModes; i++) {
		logProbOfModes[i] -= maxlogprob;
	}

	/* enforce min log prob */
	for (i = 0; i < numChangeModes; i++) {
		if (logProbOfModes[i] < k_min_log_prob) {
			logProbOfModes[i] = k_min_log_prob;
		}
	}

	mode = (EChangeModes_t)maxidx;


	if (mode == stable) {
		_data.stablecount++;

		if (_data.stablecount > CHANGE_SIGNAL_BUF_SIZE) {
			// update accumulator -- it's a moving average filter
			_data.accumulator += newx;
			_data.accumulator -= oldx;

			stablex = _data.accumulator >> CHANGE_SIGNAL_BUF_SIZE_2N;

			//record the maximum stable value every so often
			if (idx == 0) {
				UpdateMaxInBuffer(stablex);
			}



			hasStableMeas = 1;
			////print, for fun you know
			//if (_data.stablecount == 2*CHANGE_SIGNAL_BUF_SIZE) {
			//	LOGI("diff=%d\n",_data.maxStable - stablex);
			//}
		}
		else {
			//fill up accumulator
			_data.accumulator += newx;
		}
	}
	else {
		//reset, we are not stable
		_data.accumulator = 0;
		_data.stablecount = 0;
	}


	if (_data.stablecount > MAX_COUNTER) {
		_data.stablecount = MAX_COUNTER;
	}

	diffsignal = _data.maxstable - newx;

	if (diffsignal > DIFF_SIGNAL_THRESHOLD_FOR_INTERRUPTION) {
		isInterrupted = 1;
	}



#if 0
	switch (mode) {
	case stable:
		//LOGI("stable\n");
		break;

	case increasing:
		LOGI("increasing, %d\n",_data.maxstable);
		break;

	case decreasing:
		LOGI("decreasing, %d\n",_data.maxstable);
		break;

	default:
		break;
	}

#endif

	return GetGesture(isInterrupted,mode,hasStableMeas,_data.maxstable,stablex,newx);

}
