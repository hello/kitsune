#include "tinytensor_net.h"
#include "tinytensor_math.h"
#include "tinytensor_tensor.h"
#include "tinytensor_memory.h"
#include "kit_assert.h"

/*
   core neural net evalution code
   should handle all the cases, everything else is just a wrapper around this
 */

static Tensor_t * eval_all_nets(const ConstSequentialNetwork_t * net,SequentialNetworkStates_t * netstate,Tensor_t * input, const uint32_t stop_layer) {
    Tensor_t * current_input = input;
    Tensor_t * current_output = 0;
    uint32_t ilayer;
    ELayer_t prev_layer = input_layer;
    const uint32_t endidx = (net->num_layers < stop_layer || stop_layer == 0) ? net->num_layers : stop_layer;
    uint32_t output_dims[TENSOR_DIM];
    uint32_t input_dims[TENSOR_DIM];
    
    memcpy(input_dims,input->dims,sizeof(input_dims));
    
    for (ilayer = 0; ilayer < endidx; ilayer++) {
        const ConstLayer_t * const layer = &net->layers[ilayer];
        void * layer_state = NULL;
        
        if (netstate) {
            layer_state = netstate->states[ilayer];
        }

        layer->get_output_dims(layer->context,&output_dims[0],&input_dims[0]);
        
        //allocate output
        current_output = tinytensor_create_new_tensor(output_dims);
        
        //perform evaluation
        layer->eval(layer->context,layer_state,current_output,current_input,prev_layer);
        
        //output becomes new input --- so delete input if we can
        if (current_input->delete_me && current_input != input) {
            current_input->delete_me(current_input);
        }
        
        current_input = current_output;
        prev_layer = layer->layer_type;
        memcpy(input_dims, output_dims, sizeof(input_dims));

    }
    
    //whomever received this object must delete it
    return current_output;
    
}

Tensor_t * tinytensor_eval_net(const ConstSequentialNetwork_t * net,Tensor_t * input) {
    return tinytensor_eval_partial_net(net,input,0);
}

Tensor_t * tinytensor_eval_partial_net(const ConstSequentialNetwork_t * net,Tensor_t * input,const uint32_t stop_layer) {
    return eval_all_nets(net, NULL, input, stop_layer);
}

Tensor_t * tinytensor_eval_stateful_net(const ConstSequentialNetwork_t * net,SequentialNetworkStates_t * netstate, Tensor_t * input) {
    return eval_all_nets(net, netstate, input, 0);
}



void tinytensor_allocate_states(SequentialNetworkStates_t * netstate,const ConstSequentialNetwork_t * net) {
    uint32_t ilayer;
    
    assert(MAX_NUMBER_OF_LAYERS >= net->num_layers);
    
    //NULL out all pointers
    MEMSET(netstate->states,0,sizeof(netstate->states));
    
    //go through and allocate state for each layer
    //state is linked to layer via index number
    for (ilayer = 0; ilayer < net->num_layers; ilayer++) {
        
        ConstLayer_t * layer = &net->layers[ilayer];

        //only allocate if there is an allocate and free function
        if (layer->alloc_state && layer->free_state) {
            netstate->states[ilayer] = layer->alloc_state(layer->context);
        }
        
    }
}

void tinytensor_free_states(SequentialNetworkStates_t * netstate,const ConstSequentialNetwork_t * net) {
    uint32_t ilayer;

    for (ilayer = 0; ilayer < net->num_layers; ilayer++) {
        if (netstate->states[ilayer] && net->layers[ilayer].free_state) {
            
            net->layers[ilayer].free_state(net->layers[ilayer].context,&netstate->states[ilayer]);
        }
    }
}



