#ifndef _TINYTENSOR_CONV_LAYER_
#define _TINYTENSOR_CONV_LAYER_

#include "tinytensor_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONV_TENSOR_SIZE (4)
    
typedef struct {
    const ConstTensor_t * weights;
    const ConstTensor_t * biases;
    const uint32_t * output_dims;
    const uint32_t * input_dims;
    const uint32_t * max_pool_dims;
    Weight_t incoming_dropout;
    SquashFunc_t activation;

} ConvLayer2D_t;

ConstLayer_t tinytensor_create_conv_layer(const ConvLayer2D_t * static_def);

    
    
#ifdef __cplusplus
}
#endif

#endif //_TINYTENSOR_CONV_LAYER_
