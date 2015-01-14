#ifndef _AUDIOHMM_H_
#define _AUDIOHMM_H_

#include <stdint.h>


typedef struct {
    int16_t n;
    const int16_t * A;
    
    const int16_t * vecs; //unit vectors in Q10 -- length 16
    const int16_t * vars; //variances of the vectors
    
} AudioHmm_t;


//return model cost
int32_t AudioHmm_EvaluateModel(const AudioHmm_t * hmm, const int8_t * obs, const int16_t numobs);



#endif //_AUDIOHMM_H_
