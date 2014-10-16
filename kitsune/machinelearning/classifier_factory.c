#include "classifier_factory.h"
#include "linear_svm.h"
#include "hmm.h"
#include "../debugutils/matmessageutils.h"
#include <string.h>

#define BUF_SIZE (128)
#define CLASSIFIER_MAX_DATA_SIZE_BYTES (2048)

static const char * k_classifier = "classifier";
static const char * k_linear_svm = "linearsvm";
static const char * k_hmm = "hmm";

EClassifier_t ClassifierFactory_CreateFromSerializedData(void * data,Classifier_t * pClassifier,pb_istream_t * stream,uint32_t memory_limit_bytes) {
    
    /* deserialize matrix  */
    char tags[BUF_SIZE] = {0};
    char source[BUF_SIZE] = {0};
    char id[BUF_SIZE] = {0};
    int16_t myints[BUF_SIZE];
    MatDesc_t desc;
    IntArray_t arr;
    EClassifier_t type = noclassifier;

    arr.len = sizeof(myints);
    arr.type = esint16;
    arr.data.sint16 = myints;
    
    desc.source = source;
    desc.tags = tags;
    desc.id = id;
    desc.data = arr;
    
    
    if (!GetIntMatrix(&desc, stream, BUF_SIZE)) {
        return noclassifier;
    }
    
    //make sure ID says classifier
    if (strncmp(desc.id, k_classifier, BUF_SIZE) != 0) {
        return noclassifier;
    }
    
    //determine classifier type
    if (strncmp(desc.tags,k_linear_svm,BUF_SIZE) == 0) {
        LinearSvm_Init(data,myints,desc.rows,desc.cols,memory_limit_bytes);
        pClassifier->data = data;
        pClassifier->fpClassifier = LinearSvm_Evaluate;
        pClassifier->numClasses = desc.rows;
        pClassifier->numInputs = desc.cols - 1;
        pClassifier->classifiertype = linearsvm;
    }
    else if (strncmp(desc.tags,k_hmm,BUF_SIZE) == 0) {
        Hmm_Init(data, myints, desc.rows, desc.cols, memory_limit_bytes);
        pClassifier->data = data;
        pClassifier->fpClassifier = Hmm_Evaluate;
        pClassifier->numClasses = desc.rows;
        pClassifier->numInputs = desc.cols - 1;
        pClassifier->classifiertype = hmm;
    }
    
    
    
    return type;
}
