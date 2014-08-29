#include "audiofeatures.h"
#include "fft.h"
#include <string.h>
#include <stdlib.h>     /* abs */
#include "debugutils/debuglog.h"


/*
   How is this all going to work? 
   -Extract features, one of which is total energy
   -If average energy over some period is stable, then
    
    1) See if this frame of features is similar to others.
    2) If not similar, store feature vector in memory
    3) Report this frame as being 
       a. interesting, 
       b. interesting but already observed (i.e. similar)
       c. totally fucking uninteresting (ah, blissful silence)
    4) Some other piece of code will later
 
 
    -Potential pitfalls as is:
     
       impulse noises may not register at all, but will certainly be noticed by a human
       well we'll deal with that later, I can already think of a processing scheme to incorporate this.
 */
 

/*--------------------------------
 *   Memory sizes, constants, macros, and related items
 *--------------------------------*/
#define MEL_BUF_SIZE_2N (6)
#define MEL_BUF_SIZE (1 << MEL_BUF_SIZE_2N)
#define MEL_BUF_MASK (MEL_BUF_SIZE - 1)

#define CHANGE_SIGNAL_BUF_SIZE_2N (4)
#define CHANGE_SIGNAL_BUF_SIZE (1 << CHANGE_SIGNAL_BUF_SIZE_2N)
#define CHANGE_SIGNAL_BUF_MASK (CHANGE_SIGNAL_BUF_SIZE - 1)

#define MUL(a,b,q)\
  ((int16_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))

#define TOFIX(x,q)\
  ((int32_t) ((x) * (float)(1 << (q))))

#define QFIXEDPOINT (12)
#define TRUE (1)
#define FALSE (0)

/*--------------------------------
 *   Types
 *--------------------------------*/

typedef struct {
    int16_t melbuf[MEL_SCALE_ROUNDED_UP][MEL_BUF_SIZE]; //8 * 128 * 2 = 2K
    int32_t melaccumulator[MEL_SCALE_ROUNDED_UP];
    uint16_t callcounter;
    uint8_t isBufferFull;
    
    int16_t changebuf[CHANGE_SIGNAL_BUF_SIZE];
    int32_t logLikelihoodOfProbabilityRatio;
    uint8_t isStable;
    int16_t energyStable;
    uint32_t stableCount;
    
} MelFeatures_t;


typedef enum {
    eAudioSignalIsNotInteresting,
    eAudioSignalIsDiverse,
    eAudioSignalIsSimilar
} EAudioSignalSimilarity_t;

/*--------------------------------
 *   Static Memory Declarations
 *--------------------------------*/
static MelFeatures_t _data;

/* Have fun tuning these magic numbers! 
   Perhaps eventually we will have some pre-canned
   data to show you how?  */
static const int16_t k_nochange_likelihood_coefficient = TOFIX(4.0f,QFIXEDPOINT);
static const int16_t k_change_log_likelihood = TOFIX(-0.2f,QFIXEDPOINT);
static const int32_t k_max_log_likelihood_difference = TOFIX(10.0f,QFIXEDPOINT);
static const int32_t k_min_log_likelihood_difference = TOFIX(-10.0f,QFIXEDPOINT);
static const int32_t k_log_decision_threshold_of_change = TOFIX(0.0f,QFIXEDPOINT);

static const uint32_t k_stable_counts_to_be_considered_stable = 3;


/*--------------------------------
 *   Functions
 *--------------------------------*/
void AudioFeatures_Init() {
    memset(&_data,0,sizeof(_data));
}

static int16_t MovingAverage16(uint32_t counter, int16_t x,int16_t * buf, int32_t * accumulator) {
    const uint16_t idx = counter & MEL_BUF_MASK;
    
    /* accumulate */
    uint32_t a = *accumulator;
    /* add new measurement to accumulator */
    a += x;
    
    /*  remove oldest measurement from accumulator */
    a -= buf[idx];
    
    /*  store new measurement */
    buf[idx] = x;
    
    *accumulator = a;
    
    return (uint8_t) (a >> MEL_BUF_SIZE_2N);
}


/* 
 
    */

static EAudioSignalSimilarity_t CheckForSignalDiversity(const int16_t * logmfcc) {
    /*
     Now welcome to the curse of dimensionality!!!!
      Our "volume" of space will go with the the power of the dimension!
      
     So very similar feature vectors might not look similar in high dimensional
     space, but if we will apply a linear transform that we learned from the data
     to reduce the dimensionality we can then do a better job at identifiying similar 
     feature vectors
     */
    
    return eAudioSignalIsNotInteresting;
}

static void SegmentSteadyState(uint8_t isStable,const int16_t * logmfcc) {
    int16_t energySignal = logmfcc[0];
    
    if (isStable && !_data.isStable) {
        //rising edge
        _data.energyStable = energySignal;
    }
    
    
    if (!isStable && _data.isStable) {
        //falling edge
        _data.stableCount = 0;
    }
    
    if (_data.stableCount >= k_stable_counts_to_be_considered_stable) {
        
        CheckForSignalDiversity(logmfcc);
    }
    
    
    
    
    
    //increment stable count
    //just say no to rollovers.
    if ((++_data.stableCount) == 0) {
        _data.stableCount = 0xFFFFFFFF;
    };
}


uint8_t AudioFeatures_UpdateChangeSignals(const int16_t * logmfcc, uint32_t counter) {
    const uint16_t idx = counter & CHANGE_SIGNAL_BUF_MASK;
    int16_t newestenergy = logmfcc[0];
    int16_t oldestenergy = _data.changebuf[idx];
    int16_t change;
    int16_t logliknochange;
    uint8_t isStable;
   
    
    /* update buffer */
    _data.changebuf[idx] = newestenergy;

    change = newestenergy - oldestenergy;
    
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
    change = -abs(change);
    logliknochange = MUL(change,k_nochange_likelihood_coefficient,QFIXEDPOINT);
    
    
    
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
    
    _data.logLikelihoodOfProbabilityRatio += logliknochange;
    _data.logLikelihoodOfProbabilityRatio -= k_change_log_likelihood;
    
    //enforce minimum probabilties of either hypothesis
    if (_data.logLikelihoodOfProbabilityRatio > k_max_log_likelihood_difference) {
        _data.logLikelihoodOfProbabilityRatio = k_max_log_likelihood_difference;
    }
    
    if (_data.logLikelihoodOfProbabilityRatio < k_min_log_likelihood_difference) {
        _data.logLikelihoodOfProbabilityRatio = k_min_log_likelihood_difference;
    }
    
    if (_data.logLikelihoodOfProbabilityRatio > k_log_decision_threshold_of_change) {
        isStable = TRUE;
    }
    else {
        isStable = FALSE;
    }
    
    return isStable;
    
}

static void ScaleInt16Vector(int16_t * vec, uint8_t * scaling, uint16_t len,uint8_t desiredscaling) {
    uint16_t utemp16;
    uint16_t max;
    uint16_t i;
    uint8_t log2scale;
    
    /* Normalize signal  */
    max = 0;
    
    for (i = 0; i < len; i++) {
        utemp16 = abs(vec[i]);
        
        if (utemp16 > max) {
            max = utemp16;
        }
    }
    
    log2scale = desiredscaling - CountHighestMsb(max);
    
    if (log2scale > 0) {
        for (i = 0; i < len; i++) {
            vec[i] = vec[i] << log2scale;
        }
    }
    else {
        log2scale = 0;
    }
    
    *scaling = log2scale;
    
}

/* 
 *  Given AUDIO_FFT_SIZE of samples, it will return the time averaged MFCC coefficients.
 *
 *    nfftsize = log2(AUDIO_FFT_SIZE)  ergo if AUDIO_FFT_SIZE == 1024, nfftsize == 10
 * 
 *  TODO: Put in delta MFCC features, and maybe average those too.
 *
 */
#define RAW_SAMPLES_SCALE (14)

uint8_t AudioFeatures_Extract(int16_t * logmfcc, uint8_t * pIsStable,const int16_t samples[],int16_t nfftsize) {
    //enjoy this nice large stack.
    //this can all go away if we get fftr to work, and do the
    int16_t fr[AUDIO_FFT_SIZE]; //2K
    int16_t fi[AUDIO_FFT_SIZE]; //2K
    int16_t mel[MEL_SCALE_SIZE]; //inconsiquential
    uint16_t i;
    uint8_t log2scaleOfRawSignal;
    uint8_t isStable;
    int16_t mfcc[NUM_MFCC_FEATURES];
    int32_t mfccavg[NUM_MFCC_FEATURES];
    
    /* Copy in raw samples, zero out complex part of fft input*/
    memcpy(fr,samples,AUDIO_FFT_SIZE*sizeof(int16_t));
    memset(fi,0,sizeof(fi));
    
    /* Normalize time series signal  */
    ScaleInt16Vector(fr,&log2scaleOfRawSignal,AUDIO_FFT_SIZE,RAW_SAMPLES_SCALE);

    /* Get FFT */
    fft(fr,fi, AUDIO_FFT_SIZE_2N);
    
    /* Get Log Mel */
    mel_freq(mel,fr,fi,AUDIO_FFT_SIZE_2N,EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE,log2scaleOfRawSignal);
    
    DEBUG_LOG_S16("logmel",mel,MEL_SCALE_SIZE);

    /*  get dct of mel,zero padded */
    memset(fr,0,MEL_SCALE_ROUNDED_UP*sizeof(int16_t));
    memset(fi,0,MEL_SCALE_ROUNDED_UP*sizeof(int16_t));

    for (i = 0; i < MEL_SCALE_SIZE; i++) {
        fr[i] = (int16_t)mel[i];
    }
    
    
    /* fr will contain the dct */
    fft(fr,fi,NUM_MFCC_FEATURES_2N + 1);
    DEBUG_LOG_S16("mfcc",fr,NUM_MFCC_FEATURES);


    /* Moving Average */
    for (i = 0; i < NUM_MFCC_FEATURES; i++) {
        mfcc[i] = MovingAverage16(_data.callcounter,fr[i],_data.melbuf[i],&_data.melaccumulator[i]);
        
        mfccavg[i] = _data.melaccumulator[i];
    }


    DEBUG_LOG_S32("mfcc_avg",mfccavg,NUM_MFCC_FEATURES);
    
    isStable = AudioFeatures_UpdateChangeSignals(logmfcc, _data.callcounter);
    
    SegmentSteadyState(isStable,mfcc);
    
    /* Update counter */
    
    _data.callcounter++;
    
    if (_data.callcounter >= MEL_BUF_SIZE) {
        _data.isBufferFull = 1;
    }
    
    *pIsStable = isStable;
    
    memcpy(logmfcc, mfcc, MEL_SCALE_ROUNDED_UP*sizeof(int16_t));
    
    return _data.isBufferFull;
}
