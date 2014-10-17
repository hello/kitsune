#ifndef _CLASSIFIERFACTORY_H_
#define _CLASSIFIERFACTORY_H_

#include <stdint.h>
#include "machine_learning_types.h"
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif
    
EClassifier_t ClassifierFactory_CreateFromSerializedData(void * data,Classifier_t * pClassifier,pb_istream_t * stream,uint32_t memory_limit_bytes);
    
#ifdef __cplusplus
}
#endif


#endif //_CLASSIFIERFACTORY_H_