#ifndef _TINYTENSOR_MAXPOOLRELU_LAYER_H_
#define _TINYTENSOR_MAXPOOLRELU_LAYER_H_


#include "tinytensor_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif
    
typedef struct {
    const uint32_t * output_dims;
    const uint32_t * input_dims;
} MaxPoolLayer_t;
    
ConstLayer_t tinytensor_create_maxpool_layer(const MaxPoolLayer_t * static_def);
    
    
#ifdef __cplusplus
}
#endif

#endif //_TINYTENSOR_MAXPOOLRELU_LAYER_H_
