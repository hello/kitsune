#ifndef _TINYTENSOR_LSTM_LAYER_H_
#define _TINYTENSOR_LSTM_LAYER_H_

#include "tinytensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    
#define LSTM_MAX_HIDDEN_UNITS (128)


typedef struct {
    
    const ConstTensor_t * weights_input_gate;
    const ConstTensor_t * biases_input_gate;
    
    const ConstTensor_t * weights_cell;
    const ConstTensor_t * biases_cell;
    
    const ConstTensor_t * weights_forget_gate;
    const ConstTensor_t * biases_forget_gate;

    const ConstTensor_t * weights_output_gate;
    const ConstTensor_t * biases_output_gate;
    
    const uint32_t * output_dims;
    const uint32_t * input_dims;
    const Weight_t incoming_dropout;
    SquashFunc_t output_activation;
} LstmLayer_t;
    
    
typedef struct {
    int32_t * cell_state;
    Weight_t * output;
    uint32_t len;
} LstmLayerState_t;
    
ConstLayer_t tinytensor_create_lstm_layer(const LstmLayer_t * static_def);
    
#ifdef __cplusplus
}
#endif

#endif //_TINYTENSOR_LSTM_LAYER_H_
