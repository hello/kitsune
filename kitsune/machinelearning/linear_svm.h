#ifndef _LINEARSVM_H_
#define _LINEARSVM_H_

#include "machine_learning_types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t LinearSvm_Init(void * data, const int16_t * params, int32_t rows, int32_t cols,uint32_t memory_limit_bytes);

uint32_t LinearSvm_Evaluate(void * data, int8_t * classes,const int8_t * feat,uint8_t q);

#ifdef __cplusplus
}
#endif
    
#endif //_LINEARSVM_H_