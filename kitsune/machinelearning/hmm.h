#ifndef _HMM_H_
#define _HMM_H_

#include "machine_learning_types.h"

#define HMM_LOGPROB_QFIXEDPOINT (10)
#define HMM_STATETRANSITION_QFIXEDPOINT (15)


#ifdef __cplusplus
extern "C" {
#endif

uint8_t Hmm_Init(void * data, const int16_t * params, int32_t rows, int32_t cols, uint32_t memory_limit);

uint32_t Hmm_Evaluate(void * data, int8_t * classes,const int8_t * feat,uint8_t q);


#ifdef __cplusplus
}
#endif


#endif //_HMM_H_
