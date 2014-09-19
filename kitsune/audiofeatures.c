#include "audiofeatures.h"
#include "fft.h"
#include <string.h>
#include <stdlib.h>     /* abs */
#include "debugutils/debuglog.h"
#include <stdio.h>


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
#define SAMPLE_RATE_IN_HZ (EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE)
#define SAMPLE_PERIOD_IN_MILLISECONDS  (1000 / SAMPLE_RATE_IN_HZ)

#define MEL_BUF_SIZE_2N (6)
#define MEL_BUF_SIZE (1 << MEL_BUF_SIZE_2N)
#define MEL_BUF_MASK (MEL_BUF_SIZE - 1)

#define CHANGE_SIGNAL_BUF_SIZE_2N (6)
#define CHANGE_SIGNAL_BUF_SIZE (1 << CHANGE_SIGNAL_BUF_SIZE_2N)
#define CHANGE_SIGNAL_BUF_MASK (CHANGE_SIGNAL_BUF_SIZE - 1)

#define MUL(a,b,q)\
  ((int16_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))

#define MUL_PRECISE_RESULT(a,b,q)\
((int32_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))

#define TOFIX(x,q)\
  ((int32_t) ((x) * (float)(1 << (q))))

#define QFIXEDPOINT (12)
#define TRUE (1)
#define FALSE (0)

#define MAX_INT_16 (32767)

//purposely DO NOT MAKE THIS -32768
// abs(-32768) is 32768.  I can't represent this number with an int16 type!
#define MIN_INT_16 (-32767)

/* Have fun tuning these magic numbers!
 Perhaps eventually we will have some pre-canned
 data to show you how?  */

//the higher this gets, the less likely you are to be stable
static const int16_t k_stable_likelihood_coefficient = TOFIX(0.05f,QFIXEDPOINT);

//the closer this gets to zero, the more likely it is that you will be increasing or decreasing
static const int16_t k_change_log_likelihood = TOFIX(-0.05f,QFIXEDPOINT);

//the closer this gets to zero, the shorter the amount of time it will take to switch between modes
//the more negative it gets, the more evidence is required before switching modes, in general
static const int32_t k_min_log_prob = TOFIX(-1.5f,QFIXEDPOINT);

#define STABLE_TIME_TO_BE_CONSIDERED_STABLE_IN_MILLISECONDS  (500)

static const uint32_t k_stable_counts_to_be_considered_stable =  STABLE_TIME_TO_BE_CONSIDERED_STABLE_IN_MILLISECONDS / SAMPLE_PERIOD_IN_MILLISECONDS;

#define STEADY_STATE_SEGMENT_PERIOD_IN_MILLISECONDS (2000)
static const uint32_t k_stable_count_period_in_counts = STEADY_STATE_SEGMENT_PERIOD_IN_MILLISECONDS / SAMPLE_PERIOD_IN_MILLISECONDS;

#define MIN_SEGMENT_TIME_IN_MILLISECONDS (500)
static const uint32_t k_min_segment_time_in_counts = MIN_SEGMENT_TIME_IN_MILLISECONDS / SAMPLE_PERIOD_IN_MILLISECONDS;

static const int32_t k_min_energy = 10000;

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
    int16_t melbuf[NUM_MFCC_FEATURES][MEL_BUF_SIZE]; //8 * 128 * 2 = 2K
    int32_t melaccumulator[NUM_MFCC_FEATURES];
    uint16_t callcounter;
    
    int32_t changebuf[CHANGE_SIGNAL_BUF_SIZE];
    int32_t logProbOfModes[numChangeModes];
    uint8_t isStable;
    int16_t energyStable;
    uint32_t stableCount;
    uint32_t stablePeriodCounter;
    EChangeModes_t lastModes[3];
    int64_t modechangeTimes[3];
    int32_t maxabsfeatures[NUM_MFCC_FEATURES];
    uint8_t isValidSteadyStateSegment;
    

    SegmentAndFeatureCallback_t fpCallback;
    
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




/*--------------------------------
 *   Functions
 *--------------------------------*/
void AudioFeatures_Init(SegmentAndFeatureCallback_t fpCallback) {
    memset(&_data,0,sizeof(_data));
    _data.fpCallback = fpCallback;
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


static void SegmentSteadyState(EChangeModes_t currentMode,const int32_t * mfccavg,int64_t samplecount) {
    int32_t energySignal = mfccavg[0];
    uint16_t i;
    
    
    
    /*  Every day I'm segmenting segmenting
     
     
     
        If increasing, still for less than a still period, then decreasing
           then it was an audio segment, from the start of the increase to the end of the decrease
     
        If increasing, still for longer than a still period,
           then it was and audio segment, from the start of the increase to the end of the still period
     
        If still for greater than some period, and the energy was above some threshold,
           then it was an audio segment, from the start of the still period
     
     
         If audio segment, take mfc avg signal, and do a callback with segment info.
     
    */
    
    if (currentMode != stable) {
        _data.stableCount = 0;
        _data.stablePeriodCounter = 0;
    }
    else {
        if (energySignal > k_min_energy) {
            _data.isValidSteadyStateSegment = TRUE;
        }
    }
    
    //increment stable count
    if ((++_data.stableCount) == 0) {
        _data.stableCount = 0xFFFFFFFF;
    };
    
    //segment steady state (i.e. energy has been the same)
    if (_data.stableCount > k_stable_counts_to_be_considered_stable) {
        
        //if stable for long enough, erase all history
        _data.lastModes[2] = stable;
        _data.lastModes[1] = stable;
        _data.lastModes[0] = stable;
        
        //if you have been stable long enough, output a segment
        if (++_data.stablePeriodCounter > k_stable_count_period_in_counts) {
            Segment_t seg;
            seg.t1 = _data.modechangeTimes[0];
            seg.t2 = samplecount;
            seg.type = segmentSteadyState;
            
            
            if (_data.isValidSteadyStateSegment) {
                if (_data.fpCallback) {
                    _data.fpCallback(_data.maxabsfeatures,&seg);
                }
                _data.isValidSteadyStateSegment = FALSE;
            }
            
            //reset period counter
            _data.stablePeriodCounter = 0;
            
            //update mode change history
            _data.modechangeTimes[2] = _data.modechangeTimes[1];
            _data.modechangeTimes[1] = _data.modechangeTimes[0];
            _data.modechangeTimes[0] = samplecount;

            //reset features for next segment
            memset(_data.maxabsfeatures,0,sizeof(_data.maxabsfeatures));

        }
    }

    //segment out a prematurely ended steady state
    if (currentMode == increasing && currentMode != _data.lastModes[0]) {
        if (_data.lastModes[0] == stable && _data.stablePeriodCounter && _data.isValidSteadyStateSegment) {
            Segment_t seg;
            seg.t1 = _data.modechangeTimes[0];
            seg.t2 = samplecount;
            seg.type = segmentSteadyState;
            
            if (_data.fpCallback) {
                _data.fpCallback(_data.maxabsfeatures,&seg);
            }

        }
        
        _data.stablePeriodCounter = 0;
    }
    
    //segment out a burst of energy
    if ( currentMode == decreasing && currentMode != _data.lastModes[0]) {
        Segment_t seg;
        memset(&seg,0,sizeof(Segment_t));
        seg.type = segmentPacket;
        _data.stablePeriodCounter = 0;
        
        //very short
        if (_data.lastModes[0] == increasing) {
            seg.t1 = _data.modechangeTimes[0];
            seg.t2 = samplecount;
        }
        
        //little longer
        if (_data.lastModes[0] == stable && _data.lastModes[1] == increasing) {
            seg.t1 = _data.modechangeTimes[1];
            seg.t2 = samplecount;
        }
        
        //coming off of a stable period
        if (_data.lastModes[0] == stable && _data.lastModes[1] == stable) {
            seg.t1 = _data.modechangeTimes[0];
            seg.t2 = samplecount;
            seg.type = segmentSteadyState;

        }
        
        
        if (seg.t2 - seg.t1 > k_min_segment_time_in_counts) {
            
            if (_data.fpCallback) {
                _data.fpCallback(_data.maxabsfeatures,&seg);
            }
        }
        
        //reset features for next segment
        memset(_data.maxabsfeatures,0,sizeof(_data.maxabsfeatures));
        
        
    }
    
    //find max magnitude of features over time
    for (i = 0; i < NUM_MFCC_FEATURES; i++) {
        if (abs(_data.maxabsfeatures[i]) < abs(mfccavg[i])) {
            _data.maxabsfeatures[i] = mfccavg[i];
        }
    }

    //if there was a mode change, track when it happend
    if (currentMode != _data.lastModes[0]) {
        
        _data.lastModes[2] = _data.lastModes[1];
        _data.lastModes[1] = _data.lastModes[0];
        _data.lastModes[0] = currentMode;
        
        _data.modechangeTimes[2] = _data.modechangeTimes[1];
        _data.modechangeTimes[1] = _data.modechangeTimes[0];
        _data.modechangeTimes[0] = samplecount;
        
        //reset features because we may be starting a segment now
        if (currentMode == increasing) {
            memset(_data.maxabsfeatures,0,sizeof(_data.maxabsfeatures));
        }
        
        
        //printf("Mode change at %lld\n",samplecount);

    }
    
}


static void UpdateChangeSignals(EChangeModes_t * pCurrentMode, const int32_t * mfccavg, uint32_t counter) {
    const uint16_t idx = counter & CHANGE_SIGNAL_BUF_MASK;
    int32_t newestenergy = mfccavg[0];
    int32_t oldestenergy = _data.changebuf[idx];
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
    //DEBUG_LOG_S32("change32", &change32, 1);

    if (change32 < MIN_INT_16) {
        change32 = MIN_INT_16;
    }
    
    if (change32 > MAX_INT_16) {
        change32 = MAX_INT_16;
    }
    
    change16 = change32;
    //DEBUG_LOG_S16("change16", &change16, 1);

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
    
    //DEBUG_LOG_S32("logProbOfModes",NULL,logProbOfModes,3,0,0);
    //DEBUG_LOG_S16("mode",NULL,&maxidx,1,0,0);
    
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

void AudioFeatures_SetAudioData(const int16_t samples[],int16_t nfftsize,int64_t samplecount) {
    //enjoy this nice large stack.
    //this can all go away if we get fftr to work, and do the
    int16_t fr[AUDIO_FFT_SIZE]; //2K
    int16_t fi[AUDIO_FFT_SIZE]; //2K
    int16_t mel[MEL_SCALE_SIZE]; //inconsiquential
    uint16_t i;
    uint8_t log2scaleOfRawSignal;
    int32_t mfccavg[NUM_MFCC_FEATURES];
    EChangeModes_t currentMode;
    
    /* Copy in raw samples, zero out complex part of fft input*/
    memcpy(fr,samples,AUDIO_FFT_SIZE*sizeof(int16_t));
    memset(fi,0,sizeof(fi));
    
    /* Normalize time series signal  */
    ScaleInt16Vector(fr,&log2scaleOfRawSignal,AUDIO_FFT_SIZE,RAW_SAMPLES_SCALE);

    /* Get FFT */
    fft(fr,fi, AUDIO_FFT_SIZE_2N);
    
    /* Get Log Mel */
    mel_freq(mel,fr,fi,AUDIO_FFT_SIZE_2N,EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE,log2scaleOfRawSignal);
    
    //DEBUG_LOG_S16("logmel",mel,MEL_SCALE_SIZE);

    /*  get dct of mel,zero padded */
    memset(fr,0,NUM_MFCC_FEATURES*2*sizeof(int16_t));
    memset(fi,0,NUM_MFCC_FEATURES*2*sizeof(int16_t));

    for (i = 0; i < MEL_SCALE_SIZE; i++) {
        fr[i] = (int16_t)mel[i];
    }
    
    
    /* fr will contain the dct */
    fft(fr,fi,NUM_MFCC_FEATURES_2N + 1);
    DEBUG_LOG_S16("mfcc",NULL,fr,NUM_MFCC_FEATURES,samplecount,samplecount);


    /* Moving Average */
    for (i = 0; i < NUM_MFCC_FEATURES; i++) {
        MovingAverage16(_data.callcounter,fr[i],_data.melbuf[i],&_data.melaccumulator[i]);
        
        mfccavg[i] = _data.melaccumulator[i];
    }


    DEBUG_LOG_S32("mfcc_avg",NULL,mfccavg,NUM_MFCC_FEATURES,samplecount,samplecount);
    
    UpdateChangeSignals(&currentMode, mfccavg, _data.callcounter);
    
    SegmentSteadyState(currentMode,mfccavg,samplecount);
    
    /* Update counter.  It's okay if this one rolls over*/
    _data.callcounter++;
    
    
}
