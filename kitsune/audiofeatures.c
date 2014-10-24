#include "audiofeatures.h"
#include "fft.h"
#include <string.h>
#include <stdlib.h>     /* abs */
#include "debugutils/debuglog.h"
#include <stdio.h>
#include "hellomath.h"
#include "simplelink.h"  // This defines TRUE and FALSE

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
//minimum must be 1
#define MIN_ENERGY (32)

#define PSD_SIZE_2N (5)
#define PSD_SIZE (1 << PSD_SIZE_2N)

#define SAMPLE_RATE_IN_HZ (EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE)
#define SAMPLE_PERIOD_IN_MILLISECONDS  (1000 / SAMPLE_RATE_IN_HZ)

#define ENERGY_BUF_SIZE_2N (4)
#define ENERGY_BUF_SIZE (1 << ENERGY_BUF_SIZE_2N)
#define ENERGY_BUF_MASK (ENERGY_BUF_SIZE - 1)

#define CHANGE_SIGNAL_BUF_SIZE_2N (5)
#define CHANGE_SIGNAL_BUF_SIZE (1 << CHANGE_SIGNAL_BUF_SIZE_2N)
#define CHANGE_SIGNAL_BUF_MASK (CHANGE_SIGNAL_BUF_SIZE - 1)

#define STEADY_STATE_AVERAGING_PERIOD_2N (6)
#define STEADY_STATE_AVERAGING_PERIOD (1 << STEADY_STATE_AVERAGING_PERIOD_2N)

#define STARTUP_PERIOD_IN_MS (10000)
#define STARTUP_EQUALIZATION_COUNTS (STARTUP_PERIOD_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS)

#define QFIXEDPOINT (12)


/* Have fun tuning these magic numbers!
 Perhaps eventually we will have some pre-canned
 data to show you how?  */

//the higher this gets, the less likely you are to be stable
static const int16_t k_stable_likelihood_coefficient = TOFIX(1.0,QFIXEDPOINT);

//the closer this gets to zero, the more likely it is that you will be increasing or decreasing
static const int16_t k_change_log_likelihood = TOFIX(-0.10f,QFIXEDPOINT);

//the closer this gets to zero, the shorter the amount of time it will take to switch between modes
//the more negative it gets, the more evidence is required before switching modes, in general
static const int32_t k_min_log_prob = TOFIX(-0.25f,QFIXEDPOINT);

static const int16_t k_coherence_slope = TOFIX(1.0f,QFIXEDPOINT);
static const int16_t k_incoherence_slope = TOFIX(1.0f,QFIXEDPOINT);
static const int32_t k_coherence_min_log_prob = TOFIX(-0.25f,QFIXEDPOINT);

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

typedef enum {
    incoherent,
    coherent,
    numCoherencyModes
} ECoherencyModes_t;

typedef struct {
    //needed for equalization of spectrum
    int16_t lpfbuf[PSD_SIZE]; //64 bytes
    
    int16_t energybuf[ENERGY_BUF_SIZE];//32 bytes
    int32_t energyaccumulator;
    
    int16_t lastmfcc[NUM_AUDIO_FEATURES];
    
    int16_t lastEnergy;
    
    uint16_t callcounter;
    uint16_t steadyStateCounter;
    
    int16_t changebuf[CHANGE_SIGNAL_BUF_SIZE];
    int32_t logProbOfModes[numChangeModes];
    int32_t logProbOfCoherencyModes[numCoherencyModes];
    uint8_t isStable;
    int16_t energyStable;
    uint32_t stableCount;
    uint32_t stablePeriodCounter;
    EChangeModes_t lastModes[3];
    int64_t modechangeTimes[3];
    uint8_t isValidSteadyStateSegment;
    int16_t maxenergyfeatures[NUM_AUDIO_FEATURES];
    ECoherencyModes_t lastmode;
    int32_t mfccaccumulator[NUM_AUDIO_FEATURES];
    int16_t mfccavg[NUM_AUDIO_FEATURES];
    int16_t maxavgenergy;
    int16_t maxenergy;
    uint16_t psd_min_energy;
    uint16_t coherentCount;
    Segment_t coherentSegment;
    AudioFeatures_t feats;
    

    AudioFeatureCallback_t fpCallback;
    
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
void AudioFeatures_Init(AudioFeatureCallback_t fpCallback) {
    
    memset(&_data,0,sizeof(_data));
    
    _data.fpCallback = fpCallback;
    
    _data.maxavgenergy = MIN_INT_16;
    _data.maxenergy = MIN_INT_16;
    _data.psd_min_energy = MIN_ENERGY;

}

void AudioFeatures_SetParams(uint16_t psd_min_energy) {
    _data.psd_min_energy = psd_min_energy;
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

static void UpdateCoherencySignals(ECoherencyModes_t * pCurrentMode, const int16_t cosangle, uint32_t counter) {

    int32_t logLikelihoodOfModePdfs[numCoherencyModes];
    int32_t * logProbOfModes = _data.logProbOfCoherencyModes;
    int16_t i;
    int16_t coherentSignal = -((1<<10) - cosangle);
    int16_t incoherentSignal = -(cosangle);
    int32_t maxlogprob;
    int16_t maxidx;
    
    
    
 /*  Mode log pdfs
(0,0)|B\    /| (1,0)
     |  \  /A|
     |   \/  |
     |  / \  |
     | /   \ |
     |/     \|
     .       .
     .       .
  (0,-m)  (1,-n)
     
     pdf A is coherency (as cos angle goes to 1, your signal is more coherent)
  
     */
    

    
    logLikelihoodOfModePdfs[coherent] = MUL_PRECISE_RESULT(coherentSignal,k_coherence_slope,QFIXEDPOINT);
    logLikelihoodOfModePdfs[incoherent] = MUL_PRECISE_RESULT(incoherentSignal,k_incoherence_slope,QFIXEDPOINT);
    
    //DEBUG_LOG_S32("loglik", NULL, logLikelihoodOfModePdfs, numCoherencyModes, counter, counter);

    for (i = 0; i < numCoherencyModes; i++) {
        logProbOfModes[i] += logLikelihoodOfModePdfs[i];
    }
    
    
    /* normalize -- the highest log probability will now be 0 */
    maxlogprob = logProbOfModes[0];
    maxidx = 0;
    for (i = 1; i < numCoherencyModes; i++) {
        if (logProbOfModes[i] > maxlogprob) {
            maxlogprob = logProbOfModes[i];
            maxidx = i;
        }
    }
    
    for (i = 0; i < numCoherencyModes; i++) {
        logProbOfModes[i] -= maxlogprob;
    }
    
    /* enforce min log prob */
    for (i = 0; i < numCoherencyModes; i++) {
        if (logProbOfModes[i] < k_coherence_min_log_prob) {
            logProbOfModes[i] = k_coherence_min_log_prob;
        }
    }
    
    *pCurrentMode = (ECoherencyModes_t)maxidx;
    
#if 0
    switch (*pCurrentMode) {
        case coherent:
            printf("coherent\n");
            break;
            
        case incoherent:
            printf("incoherent\n");
            
            
            break;
            
        default:
            break;
    }
    
#endif
    
}

#if 0
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
#endif

static uint8_t SegmentCoherentSignals(ECoherencyModes_t mode,int16_t energy, int16_t avgenergy,const int16_t * mfcc, int64_t time) {
    uint16_t i;
    uint8_t ret = 0x00;
    
    //rising edge
    if (mode == coherent && _data.lastmode == incoherent) {
        _data.coherentSegment.t1 = time;
        _data.coherentCount = 0;
        memset(_data.mfccaccumulator,0,sizeof(_data.mfccaccumulator));
    }
    
    //falling edge
    if (mode == incoherent && _data.lastmode == coherent) {

        //compute average
        for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
            _data.mfccavg[i] = _data.mfccaccumulator[i] / _data.coherentCount;
        }
        
        _data.coherentSegment.t2 = time;
        _data.coherentSegment.duration = time - _data.coherentSegment.t1;
        _data.coherentSegment.type = segmentCoherent;
        
        //report that I have something
        ret = 0x01;
        
        
    }
    
    //average
    if (mode == coherent) {
        for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
            _data.mfccaccumulator[i] += mfcc[i];
        }
        
        if (_data.maxavgenergy < avgenergy) {
            _data.maxavgenergy = avgenergy;
        }
        
        if (_data.maxenergy < energy) {
            _data.maxenergy = energy;
        }
        
        _data.coherentCount++;
    }
    
    _data.lastmode = mode;
    
    return ret;
}


/*
 *
 */
#define RAW_SAMPLES_SCALE (14)

void AudioFeatures_SetAudioData(const int16_t samples[],int64_t samplecount) {
    //enjoy this nice large stack.
    //this can all go away if we get fftr to work, and do the
    int16_t fr[AUDIO_FFT_SIZE]; //512K
    int16_t fi[AUDIO_FFT_SIZE]; //512K
    int16_t psd[PSD_SIZE];
    int16_t dc;
    
    uint16_t i,j;
    uint8_t log2scaleOfRawSignal;
    int16_t mfcc[NUM_AUDIO_FEATURES];
    int16_t logTotalEnergy;
    int16_t logTotalEnergyAvg;

    EChangeModes_t currentMode;
    ECoherencyModes_t coherencyMode;
    uint8_t isStable;

    int16_t feats[NUM_AUDIO_FEATURES];
    int16_t vecdotresult;
    
    

    
    /* Copy in raw samples, zero out complex part of fft input*/
    memcpy(fr,samples,AUDIO_FFT_SIZE*sizeof(int16_t));
    memset(fi,0,sizeof(fi));
    
    
    
    /* Normalize time series signal  */
    //ScaleInt16Vector(fr,&log2scaleOfRawSignal,AUDIO_FFT_SIZE,RAW_SAMPLES_SCALE);
    log2scaleOfRawSignal = 0;
    
    
    /* Get FFT */
    fft(fr,fi, AUDIO_FFT_SIZE_2N);
    
    /* Get PSD, but we only care about half of the fft, so we will stop at 128 */
    logpsdmel(&logTotalEnergy,psd,fr,fi,log2scaleOfRawSignal,_data.psd_min_energy); //psd is now 64, and on a logarithmic scale after 1khz
    
    DEBUG_LOG_S16("energy", NULL, &logTotalEnergy, 1, samplecount, samplecount);

    /* Determine stability of signal energy order to figure out when to estimate background spectrum */
    logTotalEnergyAvg = MovingAverage16(_data.callcounter, logTotalEnergy, _data.energybuf, &_data.energyaccumulator,ENERGY_BUF_MASK,ENERGY_BUF_SIZE_2N);
    
    //DEBUG_LOG_S16("energyavg", NULL, &logTotalEnergyAvg, 1, samplecount, samplecount);

    
    UpdateChangeSignals(&currentMode, logTotalEnergyAvg, _data.callcounter);

    isStable = IsStable(currentMode,logTotalEnergyAvg);
    

    /* Equalize the PSD */
    
    //only do adaptive equalization if we are starting up, or our energy level is stable
    if (isStable ||  _data.callcounter < STARTUP_EQUALIZATION_COUNTS) {
        //crappy adaptive equalization. lowpass the psd, and subtract that from the psd
        for (i = 0; i < PSD_SIZE; i++) {
            _data.lpfbuf[i] = MUL(_data.lpfbuf[i], TOFIX(0.99f,15), 15) + MUL(psd[i], TOFIX(0.01f,15), 15);
        }
    }
    

    
    //get MFCC coefficients
    memset(fi,0,sizeof(fi));

    
    for (i = 0; i < PSD_SIZE; i++) {
        fr[i] = psd[i] - _data.lpfbuf[i];
    }
    
//    DEBUG_LOG_S16("psd", NULL, fr, PSD_SIZE, samplecount, samplecount);
    
    //fft of 2^8 --> 256
    dct(fr,fi,PSD_SIZE_2N);
    dc = fr[0];
    //here they are
    for (j = 0; j < NUM_AUDIO_FEATURES; j++) {
        mfcc[j] = fr[j+1];
    }
    
    //now compute MFCC directional change
    vecdotresult = cosvec16(_data.lastmfcc,mfcc,NUM_AUDIO_FEATURES);
    memcpy(_data.lastmfcc,mfcc,sizeof(_data.lastmfcc));

//    DEBUG_LOG_S16("cosvec",NULL,&vecdotresult,1,samplecount,samplecount);
//    DEBUG_LOG_S16("dc",NULL,fr,1,samplecount,samplecount);

    UpdateCoherencySignals(&coherencyMode,vecdotresult,_data.callcounter);

    //DEBUG_LOG_S16("coherent", NULL, (int16_t *)&coherencyMode, 1, samplecount, samplecount);


    /* Doing nothing with coherent signals now... :( */
    if (SegmentCoherentSignals(coherencyMode,logTotalEnergy,logTotalEnergyAvg, mfcc, samplecount)) {
        
        if (_data.callcounter > STARTUP_EQUALIZATION_COUNTS) {
//            DEBUG_LOG_S32("durations",NULL,&(_data.coherentSegment.duration),1,samplecount,samplecount);
//            DEBUG_LOG_S16("maxenergy",NULL,&(_data.maxenergy),1,samplecount,samplecount);
//            DEBUG_LOG_S16("maxavgenergy",NULL,&(_data.maxavgenergy),1,samplecount,samplecount);
//
//            printf("%lld,%lld,%d\n",_data.coherentSegment.t1,_data.coherentSegment.t2,_data.coherentSegment.duration);
//            
//            if (_data.fpCallback) {
//                _data.fpCallback(_data.mfccavg,&_data.coherentSegment);
//            }
            
            _data.maxavgenergy = MIN_INT_16;
            _data.maxenergy = MIN_INT_16;

        }
    }
    
    memcpy(&feats[0],mfcc,NUM_AUDIO_FEATURES*sizeof(int16_t));

    if (_data.callcounter > STARTUP_EQUALIZATION_COUNTS && _data.callcounter & 0x00000001) {
       //run every other count (we want to average and decimate by 2)
        int32_t temp32;
        int8_t featvec[NUM_AUDIO_FEATURES];
        int16_t featavg[NUM_AUDIO_FEATURES];
        for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
            temp32 = mfcc[i] + _data.lastmfcc[i];
            temp32 >>= 1;
            featavg[i] = (int16_t)temp32;
        }
        
        Scale16VecTo8(featvec, featavg, NUM_AUDIO_FEATURES);
        VecNormalize8(featvec, NUM_AUDIO_FEATURES);
        
        //scale down to 4 bit numbers
        for (i = 0; i < NUM_AUDIO_FEATURES; i++) {
            temp32 = featvec[i];
            temp32 += (1<<3);
            temp32 >>= 4;
            _data.feats.feats4bit[i] = (int8_t)temp32;
        }
        
        temp32 = _data.lastEnergy + logTotalEnergy;
        temp32 >>= 1;
        _data.feats.logenergy = (int16_t)temp32;
        _data.feats.logenergyOverBackroundNoise = dc;
        
        //log this only when energy is significant
        if (dc > MIN_CLASSIFICATION_ENERGY) {
            DEBUG_LOG_S8("mfcc",NULL,featvec,NUM_AUDIO_FEATURES,samplecount,samplecount);
        }

        _data.feats.samplecount = samplecount;

        //do data callback always
        if (_data.fpCallback) {
            _data.fpCallback(&_data.feats);
        }
    }
    

    DEBUG_LOG_S16("shapes",NULL,mfcc,NUM_AUDIO_FEATURES,samplecount,samplecount);

    memcpy(_data.lastmfcc,mfcc,NUM_AUDIO_FEATURES*sizeof(int16_t));
    _data.lastEnergy = logTotalEnergy;
    
    /* Update counter.  It's okay if this one rolls over*/
    _data.callcounter++;
    
    
}
