#include "audiohmm.h"
#include "../audio_types.h"
#include "../hellomath.h"

#define MAX_HMM_STATES (10)
#define QFIXEDPOINT (10)

//if Q=12, then don't divide 1.0 / (1/(2^3))
//if Q=10, 1.0 / (1/2^5)
#define MIN_DIV_LSB  (1 << (16 - QFIXEDPOINT - 1))

static void get_bmap(int32_t * bmap, const AudioHmm_t * hmm, const int8_t * obs) {
    int16_t istate,i;
    int32_t temp32;
    int16_t dotprod;
    int16_t denom;
    const int16_t * vecs;
    
    
    /*  Liklihood function -- dot products!  
        
         cos = vec * obs
     
         tan(theta/2)^2 = (1 - cos(theta)) / (1 + costheta))  ===> t2
     
         likelihood = exp(-t2 / var)
         loglik = -t2 / var
     
     */
    
    vecs = hmm->vecs;
    for (istate = 0; istate < hmm->n; istate++) {

        // dot product
        temp32 = 0;
        for ( i = 0; i < NUM_AUDIO_FEATURES; i++) {
            //4Q3 * 16Q(QFIXEDPOINT) operation
            temp32 += obs[i] * vecs[i];
        }
        
        //bring back to QFIXEDPOINT
        temp32 >>= 3;
        
        dotprod = (int16_t)temp32;
        
        //fixed point divisions
        //tantheta/2 squared
        temp32 = (TOFIX(1.0f, QFIXEDPOINT) - dotprod);
        temp32 <<= QFIXEDPOINT;
        denom = TOFIX(1.0f, QFIXEDPOINT) + dotprod;
        
        if (denom <= 0) {
            temp32 = 0x7FFFFFFF;
        }
        else {
            temp32 /=  denom;
            temp32 <<= QFIXEDPOINT;
        }
        
      
        
        temp32 /= hmm->vars[istate]; //divide by variance
        temp32 = -temp32;
        
        bmap[istate] = temp32;
        
        
        //go onto next set of vecs
        vecs += NUM_AUDIO_FEATURES;
    }
    
    
}

int32_t AudioHmm_EvaluateModel(const AudioHmm_t * hmm, const int8_t * obs, const int16_t numobs) {
    //this is just the forwards calculation of the HMM model
    //just assume pi is uniform
    int32_t a1[MAX_HMM_STATES];
    int32_t a2[MAX_HMM_STATES];
    int32_t tempvec32[MAX_HMM_STATES];

    int32_t bmap[MAX_HMM_STATES];
    const int8_t * currentobs;
    const int16_t * A;
    
    int16_t temp16;
    int32_t temp32;
    int32_t tempcost;
    int16_t i,j;
    int16_t t;
    int32_t cost = 0;
    

    if (!hmm || hmm->n <= 0 || hmm->n >= MAX_HMM_STATES) {
        return -1;
    }
    
    
    temp16 = TOFIX(1.0f, QFIXEDPOINT) / hmm->n;
    
    //set to uniform
    for (i = 0; i < hmm->n; i++) {
        a1[i] = FixedPointLog2Q10(temp16);
    }
    
    currentobs = obs;
    get_bmap(bmap,hmm,currentobs);

    for (i = 0; i < hmm->n; i++) {
        a1[i] = bmap[i] + a1[i];
    }
    
    
    for (t = 1; t < numobs; t++) {
        currentobs += NUM_AUDIO_FEATURES;
        
        get_bmap(bmap,hmm,currentobs);
        temp16 = 0;
        for (j = 0; j < hmm->n; j++) {
            A = hmm->A;
            for (i = 0; i < hmm->n; i++) {
                tempvec32[i] = a1[i] + FixedPointLog2Q10(A[j]);
                A += hmm->n; //next row
            }
            
            //normalize, exp, sum
            temp32 = INT32_MIN;
            for (i = 0; i < hmm->n; i++) {
                if (tempvec32[i] > temp32) {
                    temp32 = tempvec32[i];
                }
            }
            
            tempcost = temp32;
            
            //normalized
            for (i = 0; i < hmm->n; i++) {
                tempvec32[i] -= tempcost;
            
                if (tempvec32[i] < INT16_MIN) {
                    tempvec32[i] = INT16_MIN;
                }
            }
            
            temp32 = 0;
            for (i = 0; i < hmm->n; i++) {
                temp32 += FixedPointExp2Q10((int16_t)tempvec32[i]);
            }
            
            temp32 = FixedPointLog2Q10(temp32);
            temp32 += tempcost;
            
            a2[j] = temp32 + bmap[j];
            
        }
        
        for (j = 0; j < hmm->n; j++) {
            a1[j] = a2[j];
        }
        
    }
    
    //get max, call this the cost (normally supposed to sum these, but I'm being lazy here)
    temp32 = INT32_MIN;
    for (j = 0; j < hmm->n; j++) {
        if (a1[j] > temp32) {
            temp32 = a1[j];
        }
    }
    
    cost = temp32;
    
    temp16 = TOFIX(1.0f, QFIXEDPOINT);
    temp16 /= numobs;
    //want average cost
    cost -= FixedPointLog2Q10(temp16);

    
    return cost;
    
}
