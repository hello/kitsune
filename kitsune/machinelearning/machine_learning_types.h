#ifndef _MACHINELEARNINGTYPES_H_
#define _MACHINELEARNINGTYPES_H_

#include <stdint.h>

typedef uint32_t (*ClassifierFunc8_t)(void * data, int8_t * classes,const int8_t * feat,uint8_t q);

typedef enum {
    noclassifier,
    linearsvm,
    hmm,
    numClassifiers
} EClassifier_t;



typedef struct {
    void * data;
    ClassifierFunc8_t fpClassifier;
    
    EClassifier_t classifiertype;
    uint16_t numInputs;
    uint16_t numClasses;
    
    
} Classifier_t;

#endif //_MACHINELEARNINGTYPES_H_
