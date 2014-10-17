#include "hmm.h"
#include <string.h>
#include "hellomath.h"

#define MIN_LOG_PROB (-10000)
#define MAX_NUMBER_STATES (10)

typedef struct {
    uint8_t numObservations;
    uint8_t numStates;
    int16_t * logConditionalProbs;
    int16_t * stateTransitionMatrix;
    int16_t * logprobstate;
} Hmm_t;

//size 4 alignment? Sure.
#define MEM_BITS_2N (2)
static void memalign(void ** ptr) {
    uint64_t ptrval = (uint64_t)(*ptr);
    
    ptrval += (1<< MEM_BITS_2N);
    ptrval >>= MEM_BITS_2N;
    ptrval <<= MEM_BITS_2N;
    
    *ptr = (void *)ptrval;
}

uint8_t Hmm_Init(void * data, const int16_t * params, int32_t rows, int32_t cols, uint32_t memory_limit) {
    Hmm_t * pdata = (Hmm_t *) data;
    uint16_t j,i,k,idx;
    size_t usedmemory;

    if (sizeof(Hmm_t) > memory_limit) {
        return 0;
    }
    
    memset(pdata,0,sizeof(Hmm_t));
    
    /* We get a matrix, and we divide it up like this.
     
       [ P(obs | states)  , P(state_tk+1 | P(state_tk) ]
     
     */
    pdata->numStates = rows;
    pdata->numObservations = cols - rows;
    
    usedmemory = sizeof(Hmm_t);
    usedmemory += rows*cols*sizeof(int16_t) * pdata->numStates*sizeof(int32_t);
    
    if (usedmemory > memory_limit) {
        return 0;
    }

    
    pdata->logConditionalProbs = (int16_t *)(pdata + 1);
    pdata->stateTransitionMatrix = pdata->logConditionalProbs + (pdata->numStates * pdata->numObservations);
    memalign( (void **)&pdata->stateTransitionMatrix);
    pdata->logprobstate = pdata->stateTransitionMatrix + pdata->numStates*pdata->numStates;
    memalign( (void **)&pdata->logprobstate);

    //copy out values of matrices
    //transpose this guy
    k = 0;
    for (i = 0; i < pdata->numObservations; i++) {
        for (j = 0; j < pdata->numStates; j++) {
            idx = j*cols + i;
            pdata->logConditionalProbs[k] = params[idx];
            k++;
        }
    }
    
    k = 0;
    for (j = 0; j < pdata->numStates; j++) {
        for (i = 0; i < pdata->numStates; i++) {
            idx = j*cols + pdata->numObservations + i;
            pdata->stateTransitionMatrix[k] = params[idx];
            k++;
        }
    }
    
    for (i = 0; i < pdata->numStates - 1; i++) {
        pdata->logprobstate[i] = MIN_LOG_PROB;
    }
    
    pdata->logprobstate[pdata->numStates-1] = -1;
    
    return 1;
    
        
    
}

uint32_t Hmm_Evaluate(void * data, int8_t * classes,const int8_t * feat,uint8_t q) {
    Hmm_t * pdata = (Hmm_t *) data;
    int16_t probs[MAX_NUMBER_STATES];
    int16_t probs2[MAX_NUMBER_STATES];
    uint32_t utemp32;
    int32_t temp32;
    int16_t logtotalprob;
    
    uint16_t i,j;
    
    /* Go from log2 probabilities to probabilties */
    for (i = 0; i < pdata->numStates; i++) {
        probs[i] = FixedPointExp2Q10(pdata->logprobstate[i]);
    }
    
    /*  Propagate */
    MatMul(probs2, pdata->stateTransitionMatrix, probs, pdata->numStates, pdata->numStates, 1, HMM_STATETRANSITION_QFIXEDPOINT);
    
    /*  Back to log land */
    for (i = 0; i < pdata->numStates; i++) {
        pdata->logprobstate[i] = FixedPointLog2Q10(probs2[i]);
    }
    
    /* If features were passed in... do an update */
    if (feat) {
        int16_t * logcondprobs = pdata->logConditionalProbs;

        for (j = 0; j < pdata->numObservations; j++) {
            
            /* If this a positive classification, we update the log probabilities accordingly */
            if (feat[j] > 0) {
                for (i = 0; i < pdata->numStates; i++) {
                    pdata->logprobstate[i] += logcondprobs[i];
                }
            }
            
            logcondprobs += pdata->numStates;
        }
    }
    
    /* Normalize probabilities */
    utemp32 = 0;
    for (i = 0; i < pdata->numStates; i++) {
        utemp32 += FixedPointExp2Q10(pdata->logprobstate[i]);
    }
    
    logtotalprob = FixedPointLog2Q10(utemp32);
    
    for (i = 0; i < pdata->numStates; i++) {
        temp32 = pdata->logprobstate[i] - logtotalprob;
        if (temp32 < MIN_LOG_PROB) {
            temp32 = MIN_LOG_PROB;
        }
        
        if (temp32 > 0) {
            temp32 = 0;
        }
        pdata->logprobstate[i] = temp32;
    }
    
    for (i = 0; i < pdata->numStates; i++) {
        temp32 = FixedPointExp2Q10(pdata->logprobstate[i]);
        temp32 >>= 3; //Q7 from Q10
        if (temp32 > INT8_MAX) {
            temp32 = INT8_MAX;
        }
        
        if (temp32 < 0) {
            temp32 = 0;
        }
        
        classes[i] = temp32;
    }
    
    
    return 0;
}