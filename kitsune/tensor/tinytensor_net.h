#ifndef _TINYTENSOR_NET_H_
#define _TINYTENSOR_NET_H_

#include "tinytensor_types.h"
#include "tinytensor_tensor.h"

#define MAX_OUTPUT_SIZE (256)
#define MAX_NUMBER_OF_LAYERS (16)

#ifdef __cplusplus
extern "C" {
#endif

    
typedef struct {
    ConstLayer_t * layers;
    uint32_t num_layers;
} ConstSequentialNetwork_t;
    
//ah a goddamned linked list!
typedef struct {
    void * states[MAX_NUMBER_OF_LAYERS]; //better this than a linked list
} SequentialNetworkStates_t ;

Tensor_t * tinytensor_eval_net(const ConstSequentialNetwork_t * net,Tensor_t * input,const uint32_t flags);

Tensor_t * tinytensor_eval_partial_net(const ConstSequentialNetwork_t * net,Tensor_t * input,const uint32_t stop_layer,const uint32_t flags);

Tensor_t * tinytensor_eval_stateful_net(const ConstSequentialNetwork_t * net,SequentialNetworkStates_t * netstate,Tensor_t * input,const uint32_t flags);
    
void tinytensor_allocate_states(SequentialNetworkStates_t * states,const ConstSequentialNetwork_t * net);

void tinytensor_free_states(SequentialNetworkStates_t * states,const ConstSequentialNetwork_t * net);
    
#ifdef __cplusplus
}
#endif



#endif //_TINYTENSOR_NET_H_
