#include "tinytensor_lstm_layer.h"
#include "tinytensor_memory.h"
#include "tinytensor_math.h"
#include "kit_assert.h"

#define DAMPING_FACTOR (247)

typedef enum {
    inputgate,
    forgetgate,
    celloutput,
    outputgate,
    NUM_GATES
} Gates_t;




static void get_output_size(const void * context,uint32_t * dims,const uint32_t * input_size) {
    const LstmLayer_t * lstm_layer = (const LstmLayer_t *) context;
    
    MEMCPY(dims,lstm_layer->output_dims,TENSOR_DIM*sizeof(uint32_t));
    dims[2] = input_size[2]; //keep time size
    
}


static void * alloc_state(const void * context) {
    const LstmLayer_t * lstm_layer = (const LstmLayer_t *) context;
    const uint32_t num_hidden_units = lstm_layer->output_dims[3];
    
    LstmLayerState_t * state = MALLOC(sizeof(LstmLayerState_t));
    state->cell_state = MALLOC(num_hidden_units * sizeof(int32_t));
    state->output = MALLOC(num_hidden_units * sizeof(Weight_t));
    state->len = num_hidden_units;
    
    memset(state->cell_state,0,num_hidden_units * sizeof(int32_t));
    memset(state->output,0,num_hidden_units * sizeof(Weight_t));

    return state;
}

static void free_state(const void * context, void ** state) {
	LstmLayerState_t * s = *state;
	FREE(s->cell_state);
	FREE(s->output);
    FREE(s);
    *state = NULL;
}

__attribute__((section(".ramcode")))
static int16_t hard_sigmoid(int32_t x,int8_t in_scale) {
    
    int32_t temp32 = x * 6554; //0.2 Q15 * x Q7 = 0.2x Q22
    
    if (in_scale > 0) {
        temp32 >>= in_scale;
    }
    else if (in_scale < 0) {
        temp32 <<= -in_scale;
    }
    
    temp32 += 2097152; //0.5 in Q22
    temp32 += 16384;//round
    temp32 >>= 15; //back to Q7
    
    temp32 = temp32 > (1 << QFIXEDPOINT) ? (1 << QFIXEDPOINT) : temp32;
    temp32 = temp32 < 0 ? 0 : temp32;
    
    return (int16_t) temp32;
}

#include "FreeRTOS.h"
#include "task.h"
#include "uart_logger.h"
__attribute__((section(".ramcode")))
static void lstm_time_step_forwards(int32_t * cell_state,
                                    Weight_t * output,
                                    const Weight_t * input_vec,
                                    const Weight_t * weights[NUM_GATES],
                                    const Weight_t * biases[NUM_GATES],
                                    const int8_t weights_scale[NUM_GATES],
                                    const int8_t biases_scale[NUM_GATES],
                                    const uint32_t num_cells,
                                    const uint32_t num_inputs,
                                    const int8_t input_scale,
                                    SquashFunc_t output_activation) {
    
    
  
    uint32_t igate;
    uint32_t icell;
    int32_t accumulator32;
    int32_t temp32;
    int8_t temp8;
    Weight_t h;
    int8_t tempscale;
    int32_t bias32;

    const Weight_t * weight_row_starts[NUM_GATES];
    const Weight_t * bias_row_starts[NUM_GATES];
    const uint32_t total_len = num_cells + num_inputs;
    int32_t pre_activations[NUM_GATES];

    int16_t activation_forget_gate;
    int16_t activation_input_gate;
    int16_t activation_output_gate;
    Weight_t activation_cell;
    
    for (igate = 0; igate < NUM_GATES; igate++) {
        //set up row starts for all weights
        weight_row_starts[igate] = weights[igate];
        bias_row_starts[igate] = biases[igate];
    }
    
    
    for (icell = 0; icell < num_cells; icell++) {
//        printf("cell=%d\n",icell);
        
        if (icell == 3) {
            int foo = 3;
            foo++;
        }

        for (igate = 0; igate < NUM_GATES; igate++) {

            accumulator32 = 0;
            const Weight_t * w = weight_row_starts[igate];
            const Weight_t * b = bias_row_starts[igate];
            const int8_t w_scale = weights_scale[igate];
            const int8_t b_scale = biases_scale[igate];
            
            accumulator32 = accumulate(total_len,w,input_vec);
            
            bias32 = *b << QFIXEDPOINT; //to Q14 + QB FROM Q7 + QB
            
            temp8 = b_scale - input_scale - w_scale;
            
            if (temp8 > 0) {
                bias32 >>= temp8;
            }
            else if (temp8 < 0){
                bias32 <<= -temp8;
            }
            
            accumulator32 += bias32;
            
            if (w_scale > 0) {
                accumulator32 >>= w_scale;
            }
            else if (w_scale < 0) {
                accumulator32 <<= -w_scale;
            }
            
            //Q14 + QI ---> Q7 + QI
            accumulator32 += (1 << (QFIXEDPOINT - 1));
            accumulator32 >>= QFIXEDPOINT;
            pre_activations[igate] = accumulator32;
            
            //update indices
            weight_row_starts[igate] += total_len;
            bias_row_starts[igate] += 1;

        }
        

        //now that we have our activations, process the gates
        activation_forget_gate = hard_sigmoid(pre_activations[forgetgate],input_scale);
        activation_input_gate = hard_sigmoid(pre_activations[inputgate],input_scale);
        activation_output_gate = hard_sigmoid(pre_activations[outputgate],input_scale);
        
        output_activation(&activation_cell,&tempscale,pre_activations[celloutput],input_scale);
        assert(tempscale == 0);

        
        //apply forget gate to prev cell state
        temp32 = activation_forget_gate * cell_state[icell]; //Q7 x Q14 ---> Q21
        temp32 >>= QFIXEDPOINT; //Q14
        
        //and add gated cell input
        temp32 += (int32_t)activation_input_gate * (int32_t)activation_cell; //Q7 * Q7 --->Q14
        
        cell_state[icell] = temp32; //save result
        
        ///////////////////
        //output hidden state
        //take cell output "c", apply activation function to it
        
        //To Q7
        temp32 >>= QFIXEDPOINT;
        
        output_activation(&h,&tempscale,temp32,0);
        assert(tempscale == 0);
        //Q7 x Q7  = Q14
        temp32 = (int16_t)h * (int16_t)activation_output_gate;
        
        temp32 >>= QFIXEDPOINT;
        
        if (temp32 > MAX_WEIGHT) {
            temp32 = MAX_WEIGHT;
        }
        
        if (temp32 < -MAX_WEIGHT) {
            temp32 = -MAX_WEIGHT;
        }
        
        
        output[icell] = (Weight_t)temp32;
        
    
    }


   
}



/*
 
 The general idea here is that there is a bunch of vector inputs
 The dims are 1 x 1 x T x N
 // T is length of vector sequence
 // N is vector length
 //
 // the output is the number of hidden units in this layer
 // it's dims are [1 x 1 x T x M]
 //
 // so total input is [1 x 1 x T x (M + N)]
 // and output is [1 x 1 x T x M]
 //
 // cell state is intialized to zero
 
 */

__attribute__((section(".ramcode")))
static void eval_helper(const void * context, Tensor_t * out,const Tensor_t * in,ELayer_t prev_layer_type,
                        int32_t * cell_state, Weight_t * prev_hidden, uint8_t is_stateful,const uint32_t flags) {
    
    const LstmLayer_t * lstm_layer = (const LstmLayer_t *) context;
    
    //arrange weights `n stuff
    const Weight_t * weights[NUM_GATES] = {
        lstm_layer->weights_input_gate->x,
        lstm_layer->weights_forget_gate->x,
        lstm_layer->weights_cell->x,
        lstm_layer->weights_output_gate->x};
    
    const int8_t weight_scales[NUM_GATES] = {
        lstm_layer->weights_input_gate->scale,
        lstm_layer->weights_forget_gate->scale,
        lstm_layer->weights_cell->scale,
        lstm_layer->weights_output_gate->scale};

    
    const Weight_t * biases[NUM_GATES] = {
        lstm_layer->biases_input_gate->x,
        lstm_layer->biases_forget_gate->x,
        lstm_layer->biases_cell->x,
        lstm_layer->biases_output_gate->x};
    
    const int8_t bias_scales[NUM_GATES] = {
        lstm_layer->biases_input_gate->scale,
        lstm_layer->biases_forget_gate->scale,
        lstm_layer->biases_cell->scale,
        lstm_layer->biases_output_gate->scale};
    

    const uint32_t time_length = is_stateful ? 1 : in->dims[2];
    const uint32_t num_inputs = in->dims[3];
    const uint32_t num_hidden_units = lstm_layer->output_dims[3];
    uint32_t t;
    uint32_t i;
    int32_t temp32;
    
    uint8_t current_gate = 0;
    Weight_t * out_row = out->x;
    const Weight_t * in_row = in->x;
    

    //our "large" stack variable
    //which isn't that large
    Weight_t input[LSTM_MAX_HIDDEN_UNITS];
    
    for (t = 0; t < time_length; t++) {

        MEMCPY(input,in_row,num_inputs*sizeof(Weight_t));
        MEMCPY(input + num_inputs,prev_hidden,num_hidden_units*sizeof(Weight_t));
        
        lstm_time_step_forwards(cell_state,
                                prev_hidden,
                                input,
                                weights,
                                biases,
                                weight_scales,
                                bias_scales,
                                num_hidden_units,
                                num_inputs,
                                in->scale,
                                lstm_layer->output_activation);
        

        if (flags & NET_FLAG_LSTM_DAMPING) {
            for (i = 0; i < num_hidden_units; i++) {
                int64_t temp64 = cell_state[i] * DAMPING_FACTOR;
                temp64 >>= 8;
                cell_state[i] = (int32_t)temp64;
            }
        }
     
        /*
        for (i = 0; i < num_hidden_units; i++) {
            if (i != 0) printf(",");
            printf("%d",cell_state[i] >> QFIXEDPOINT);
        }
        printf("\n");
        */
        
        MEMCPY(out_row,prev_hidden,num_hidden_units * sizeof(Weight_t));

        current_gate++;
        out_row += num_hidden_units;
        in_row += num_inputs;
    }
    
    
    /*
    printf("\n");
    printf("\n");
    printf("\n");
*/

}

__attribute__((section(".ramcode")))
static void eval(const void * context,void * layer_state,Tensor_t * out,const Tensor_t * in,ELayer_t prev_layer_type,const uint32_t flags) {
    LstmLayerState_t * state = (LstmLayerState_t *)layer_state;

    //const void * context, Tensor_t * out,const Tensor_t * in,ELayer_t prev_layer_type, int32_t * cell_state, Weight_t * prev_hidden
    if (state) {
        eval_helper(context,out,in,prev_layer_type,state->cell_state,state->output,1,flags);
    }
    else {
        
        //WE HAVE THIS BRANCH SEPARATE IN CASE WE ARE NOT USING A STATEFUL LSTM
        //THIS LETS US SAVE ON STACK SIZE IN THE STATEFUL CASE
        int32_t cell_state[LSTM_MAX_HIDDEN_UNITS];
        Weight_t prev_hidden[LSTM_MAX_HIDDEN_UNITS];
        
        MEMSET(cell_state,0,sizeof(cell_state));
        MEMSET(prev_hidden,0,sizeof(prev_hidden));
        
        eval_helper(context,out,in,prev_layer_type,cell_state,prev_hidden,0,flags);
    }
    
}
__attribute__((section(".ramcode")))
ConstLayer_t tinytensor_create_lstm_layer(const LstmLayer_t * static_def) {
    ConstLayer_t layer = {eval,get_output_size,lstm_layer,static_def,alloc_state,free_state};
    return layer;
}
