#include "audiofeatures.h"
#include "fft.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>     /* abs */
#include "debugutils/debuglog.h"
#include <stdio.h>
#include "hellomath.h"
#include "tensor/features_types.h"
#ifdef USED_ON_DESKTOP
#define LOGA(...)
#include <assert.h>
#else
#include "uart_logger.h"
#include "kit_assert.h"
#endif

#define TOFIX(x,q)\
        ((int32_t) ((x) * (float)(1 << (q))))


/*--------------------------------
 *   Memory sizes, constants, macros, and related items
 *--------------------------------*/
//minimum must be 1
#define MIN_ENERGY (32)

#define PSD_SIZE_2N (5)
#define PSD_SIZE (1 << PSD_SIZE_2N)

#define ENERGY_BUF_SIZE_2N (4)
#define ENERGY_BUF_SIZE (1 << ENERGY_BUF_SIZE_2N)
#define ENERGY_BUF_MASK (ENERGY_BUF_SIZE - 1)

#define CHANGE_SIGNAL_BUF_SIZE_2N (5)
#define CHANGE_SIGNAL_BUF_SIZE (1 << CHANGE_SIGNAL_BUF_SIZE_2N)
#define CHANGE_SIGNAL_BUF_MASK (CHANGE_SIGNAL_BUF_SIZE - 1)

#define QFIXEDPOINT (12)

#define TRUE (1)
#define FALSE (0)

#define MICROPHONE_CALIBRATION_OFFSET (56.0f)


//the higher this gets, the less likely you are to be stable
static const int16_t k_stable_likelihood_coefficient = TOFIX(1.0,QFIXEDPOINT);

//the closer this gets to zero, the more likely it is that you will be increasing or decreasing
static const int16_t k_change_log_likelihood = TOFIX(-1.0f,QFIXEDPOINT);

//the closer this gets to zero, the shorter the amount of time it will take to switch between modes
//the more negative it gets, the more evidence is required before switching modes, in general
static const int32_t k_min_log_prob = TOFIX(-0.25f,QFIXEDPOINT);

#define STABLE_TIME_TO_BE_CONSIDERED_STABLE_IN_MILLISECONDS  (2000)

static const uint32_t k_stable_counts_to_be_considered_stable =  STABLE_TIME_TO_BE_CONSIDERED_STABLE_IN_MILLISECONDS / SAMPLE_PERIOD_IN_MILLISECONDS;



/*--------------------------------
 *   Types
 *--------------------------------*/
typedef enum {
    stable,
    increasing,
    decreasing,
    numChangeModes
} EChangeModes_t;

typedef struct {
    int16_t energybuf[ENERGY_BUF_SIZE];//32 bytes
    int32_t energyaccumulator;
    
    int16_t lastEnergy;
    
    uint16_t callcounter;
    uint16_t steadyStateCounter;
    
    int16_t changebuf[CHANGE_SIGNAL_BUF_SIZE];
    int32_t logProbOfModes[numChangeModes];
    uint8_t isStable;
    int16_t energyStable;
    uint32_t stableCount;
    uint32_t stablePeriodCounter;
    uint16_t min_energy;
    uint8_t statsLastIsStable;
    int16_t maxenergy;

    AudioEnergyStatsCallback_t fpEnergyStatsResultsCallback;
    
} SimpleAudioFeatures_t;


/*--------------------------------
 *   Static Memory Declarations
 *--------------------------------*/
static SimpleAudioFeatures_t _data;




/*--------------------------------
 *   Functions
 *--------------------------------*/
void init_background_energy(AudioEnergyStatsCallback_t fpOncePerMinuteCallback) {
    
    memset(&_data,0,sizeof(_data));
    
    _data.fpEnergyStatsResultsCallback = fpOncePerMinuteCallback;

    _data.min_energy = MIN_ENERGY;

}

static int32_t GetAudioEnergyAsDBA(int16_t logenergy) {
	int32_t dba = 0;

	//account for hanning window energy reduction
	dba += TOFIX(LOG2_HANNING_WINDOW_ENERGY_MULTIPLIER,10);

	//add energy
	dba += logenergy;

	//convert from log2 to 10*log10 (note not 20log10, because we are already using units of energy)
	dba *= 3;

	//add microphone noise floor (measured)
	dba += TOFIX(MICROPHONE_CALIBRATION_OFFSET,10);

	return dba;
}

static int16_t MovingAverage16(uint32_t counter, const int16_t x,int16_t * buf, int32_t * accumulator,const uint32_t mask,const uint8_t shiftnum) {
    const uint16_t idx = counter & mask;
    
    /* accumulate */
    uint32_t a = *accumulator;
    /* add new measurement to accumulator */
    a += x;
    
    /*  remove oldest measurement from accumulator */
    a -= buf[idx];
    
    /*  store new measurement */
    buf[idx] = x;
    
    *accumulator = a;
    
    return (int16_t) (a >> shiftnum);
}

//finds stats of a disturbance, and performs callback when distubance is over
static void UpdateEnergyStats(EChangeModes_t changeMode, uint8_t isStable,int16_t logTotalEnergyAvg,int16_t logTotalEnergy) {
	AudioEnergyStats_t data;
	memset(&data,0,sizeof(data));

	//leaving stable mode -- therefore starting a disturbance
	if (!isStable && _data.statsLastIsStable) {
		//DISP("S->US\r\n");
		_data.maxenergy = logTotalEnergy;
	}

	if (!isStable) {
		if (logTotalEnergy > _data.maxenergy) {
			_data.maxenergy = logTotalEnergy;
		}
	}

	//entering stable mode --ending a disturbance
	if (isStable && !_data.statsLastIsStable ) {
		//DISP("US->S\r\n");
		data.num_disturbances = 1;
	}

	if (_data.fpEnergyStatsResultsCallback) {
		data.disturbance_time_count = changeMode == stable ? 0 : SAMPLE_PERIOD_IN_MILLISECONDS;
		data.peak_background_energy = GetAudioEnergyAsDBA(logTotalEnergyAvg);
		data.peak_energy = GetAudioEnergyAsDBA(logTotalEnergy);
		_data.fpEnergyStatsResultsCallback(&data);
	}


	_data.statsLastIsStable = isStable;
}


static uint8_t IsStable(EChangeModes_t currentMode,const int16_t energySignal) {
    
    uint8_t isStable = FALSE;
    
    if (currentMode != stable) {
        _data.stableCount = 0;
    }
    else {
        
        //increment stable count
        if ((++_data.stableCount) == 0) {
            _data.stableCount = 0xFFFFFFFF;
        };
        
    }
    
    //segment steady state (i.e. energy has been the same)
    if (_data.stableCount > k_stable_counts_to_be_considered_stable) {
        isStable = TRUE;
    }

    return isStable;
    
}


static void UpdateChangeSignals(EChangeModes_t * pCurrentMode, const int16_t newestenergy, uint32_t counter) {
    const uint16_t idx = counter & CHANGE_SIGNAL_BUF_MASK;
    const int32_t oldestenergy = _data.changebuf[idx];
    int32_t change32;
    int16_t change16;
    int32_t logLikelihoodOfModePdfs[numChangeModes];
    int32_t * logProbOfModes = _data.logProbOfModes;
    int16_t i;
    int16_t maxidx;
    int32_t maxlogprob;
    
   
    
    /* update buffer */
    _data.changebuf[idx] = newestenergy;

    change32 = (int32_t)newestenergy - (int32_t)oldestenergy;
    
    if (change32 < MIN_INT_16) {
        change32 = MIN_INT_16;
    }
    
    if (change32 > MAX_INT_16) {
        change32 = MAX_INT_16;
    }
    
    change16 = change32;
    //DEBUG_LOG_S16("change", NULL, &change16, 1, counter, counter);
    
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
    //DEBUG_LOG_S32("loglik", NULL, logLikelihoodOfModePdfs, 3, counter, counter);
    
    
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
    
    *pCurrentMode = (EChangeModes_t)maxidx;
    
#if 0
    switch (*pCurrentMode) {
        case stable:
            printf("stable\n");
            break;
            
        case increasing:
            printf("increasing\n");

            
            break;
            
        case decreasing:
            
            printf("decreasing\n");

            break;
            
            default:
            break;
    }
    
#endif
    
}


__attribute__((section(".ramcode")))
static void getvolume(int16_t * logTotalEnergy, const int16_t * fr,const int16_t * fi,uint16_t min_energy, const int16_t log2scale) {
    uint64_t utemp64;
    uint64_t non_weighted_energy = 0;
    uint64_t a_weighted_energy = 0;
    int32_t temp32;
    
    //from 0 - 8Khz
    const static uint16_t a_weight_q10[128] = { 0, 0, 100, 150, 263, 379, 489,
                        510, 725, 763, 823, 859, 896, 934, 963, 994, 1024, 1054, 1085, 1093,
                        1101, 1110, 1123, 1136, 1149, 1152, 1156, 1159, 1162, 1166, 1169,
                        1172, 1176, 1166, 1170, 1174, 1178, 1182, 1184, 1185, 1187, 1188,
                        1189, 1185, 1180, 1176, 1171, 1167, 1162, 1162, 1162, 1162, 1162,
                        1162, 1162, 1162, 1161, 1159, 1157, 1156, 1154, 1152, 1151, 1149,
                        1146, 1142, 1139, 1136, 1133, 1129, 1126, 1123, 1120, 1116, 1112,
                        1107, 1103, 1098, 1094, 1089, 1085, 1081, 1076, 1072, 1067, 1063,
                        1059, 1054, 1050, 1046, 1042, 1037, 1033, 1029, 1025, 1021, 1016,
                        1012, 1012, 1009, 1005, 1002, 998, 995, 991, 987, 984, 981, 977,
                        974, 970, 967, 963, 959, 956, 952, 948, 945, 941, 937, 934, 930,
                        927, 923, 920, 916, 913, 913 };

    uint16_t idx, ifft;

    const int16_t idx_shift = FEATURES_FFT_SIZE_2N - 8;

    for (ifft = 1; ifft < FEATURES_FFT_SIZE/2; ifft++) {
    	utemp64 = 0;
    	utemp64 += (int32_t)fr[ifft]*(int32_t)fr[ifft];
    	utemp64 += (int32_t)fi[ifft]*(int32_t)fi[ifft];

    	idx = ifft >> idx_shift;
    	assert(idx < 128);

    	a_weighted_energy += (utemp64 * a_weight_q10[idx]) >> 10;
    	non_weighted_energy += utemp64;
    }


    temp32 =  FixedPointLog2Q10(a_weighted_energy + min_energy) - 2 * log2scale*1024;

    if (temp32 > INT16_MAX) {
        temp32 = INT16_MAX;
    }

    if (temp32 < INT16_MIN) {
        temp32 = INT16_MIN;
    }

    *logTotalEnergy = (int16_t)temp32;

}


void set_background_energy(const int16_t fr[], const int16_t fi[], int16_t log2scale) {
    int16_t logTotalEnergy = 0;
    int16_t logTotalEnergyAvg = 0;
    EChangeModes_t currentMode;
    uint8_t isStable;

    /* compute volume */
    getvolume(&logTotalEnergy,fr,fi,_data.min_energy,log2scale);

    /* Determine stability of signal energy order to figure out when to estimate background spectrum */
    logTotalEnergyAvg = MovingAverage16(_data.callcounter, logTotalEnergy, _data.energybuf, &_data.energyaccumulator,ENERGY_BUF_MASK,ENERGY_BUF_SIZE_2N);

    /*
     *  Determine stability of average energy -- we will use this to determine when it's safe to assume
     *   that all the audio we are hearing is just background noise
     */
    UpdateChangeSignals(&currentMode, logTotalEnergyAvg, _data.callcounter);
    isStable = IsStable(currentMode,logTotalEnergyAvg);


    UpdateEnergyStats(currentMode,isStable,logTotalEnergyAvg,logTotalEnergy);

    _data.lastEnergy = logTotalEnergy;

    _data.callcounter++;

    /*
    if (_data.callcounter % 66 == 0) {
       	DISP("vol_energy=%d\r\n",GetAudioEnergyAsDBA(logTotalEnergyAvg));
    }
*/

}
