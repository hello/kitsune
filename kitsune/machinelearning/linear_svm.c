#include "linear_svm.h"
#include <string.h>
typedef struct {

    int32_t numclasses;
    int32_t numinputs;
    int16_t * params;
    
} LinearSVM_t;

/* Yay for janky memory management */
uint32_t LinearSvm_Init(void * data, const int16_t * params, int32_t rows, int32_t cols, uint32_t memory_limit) {
    uint32_t size = sizeof(LinearSVM_t) + rows*cols*sizeof(int16_t) + 4;
    LinearSVM_t * pdata = (LinearSVM_t *) data;
    
    if (size > memory_limit) {
        return 0;
    }
    
    //golly, this is easier in C++
    memset(data,0,sizeof(LinearSVM_t)); //zero out struct memory
    pdata->numclasses = rows;
    pdata->numinputs = cols - 1;
    
    //assigned pointer blob after the main struct
    //this should be memaligned okay.
    pdata->params = (int16_t *) ( pdata + 1 );
    memcpy(pdata->params,params,rows*cols*sizeof(int16_t));
    
    return size;
}


uint32_t LinearSvm_Evaluate(void * data, int8_t * classes,const int8_t * feat,uint8_t q) {
    LinearSVM_t * pdata = (LinearSVM_t *) data;
    
    int32_t temp32;
    uint16_t i,j;
    int16_t * vec;
    
    for (j = 0; j < pdata->numclasses; j++) {
        classes[j] = 0;
    }
    
    if (feat) {
        //just matrix multiplication folks
        for (j = 0; j < pdata->numclasses; j++) {
            temp32 = 0;
            vec = pdata->params + j*(pdata->numinputs + 1);
            for (i = 0; i < pdata->numinputs; i++) {
                temp32 += (int32_t)feat[i] * (int32_t)vec[i];
                
            }
            
            temp32 >>= q;
            temp32 += vec[i];
            
            if (temp32 > 0) {
                classes[j] = 1;
            }
            else {
                classes[j] = -1;
            }
        }
    }
    

    return 0;
}
