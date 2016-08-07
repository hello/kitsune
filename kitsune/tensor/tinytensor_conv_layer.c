#include "tinytensor_conv_layer.h"
#include "tinytensor_memory.h"
#include "tinytensor_math.h"
#include <assert.h>

#define MAX_MAX_POOL_SIZE (8)

static void get_conv2d_output_size(const void * context,uint32_t * dims,const uint32_t * input_dims) {
    const ConvLayer2D_t * layer = (const ConvLayer2D_t *)context;
    
    uint32_t i;
    for (i = 0; i < TENSOR_DIM; i++) {
        dims[i] = layer->output_dims[i];
    }
    
}



//takes two 3 dimensional tensors (a bunch of images, and a bunch of convolutional filters),
//applies them accross each of the innermost tensor dimension (i.e. A1 x B1 x C1, A2 x B2 x C2, we're talking about A1 and A2, and A1 == A2)
int8_t tinytensor_convolve3d_direct_maxpooling(int8_t * descale,
                                             Weight_t * const out_begin,
                                             Weight_t * out,
                                             const uint32_t * pool_dims,
                                             const Weight_t * weights,
                                             int8_t weight_scaling,
                                             const Weight_t * image,
                                             int8_t input_scaling,
                                             const Weight_t bias,
                                             int8_t bias_scaling,
                                             const uint32_t num_weights_rows,
                                             const uint32_t num_weights_cols,
                                             const uint32_t num_image_rows,
                                             const uint32_t num_image_cols,
                                             const uint32_t num_images,
                                             const Weight_t incoming_dropout) {
    
    
    int8_t temp_scale = 0;
    
    const uint32_t num_rows_out = num_image_rows - num_weights_rows + 1;
    const uint32_t num_cols_out = num_image_cols - num_weights_cols + 1;
    const uint32_t weight_size = num_weights_rows * num_weights_cols;
    const uint32_t image_size = num_image_rows * num_image_cols;
    
    const uint32_t num_pool_rows = num_rows_out / pool_dims[0];
    const uint32_t num_pool_cols = num_cols_out / pool_dims[1];
    
    
    
    uint32_t ioutrow,ioutcol;
    uint32_t iimage;
    uint32_t j,i;
    uint32_t ipool_row,ipool_col;
    
    
    int32_t max_pool[MAX_MAX_POOL_SIZE][MAX_MAX_POOL_SIZE];
    int32_t temp32;
    int64_t temp64;
    int32_t bias32;
    int8_t bias_scaling_diff;
    int8_t delta_descale;
    Weight_t * p;
    
    //FOR SOME REASON KERAS DOES NOT USE THE DROPOUTS FOR CONV LAYER EVALUATIONS
    //const int16_t dropout_weight = (1 << QFIXEDPOINT) - incoming_dropout;
    const int16_t dropout_weight = 128; //so just set it to 1.0
    
    Weight_t * pout = out;
    
    for (ipool_row = 0; ipool_row < num_pool_rows; ipool_row++) {
        for (ipool_col = 0; ipool_col < num_pool_cols; ipool_col++)
        {
            const uint32_t outrow_start = ipool_row * pool_dims[0];
            const uint32_t outcol_start = ipool_col * pool_dims[1];
            const uint32_t outrow_end = outrow_start + pool_dims[0];
            const uint32_t outcol_end = outcol_start + pool_dims[1];
            
            
            for (ioutrow = outrow_start; ioutrow < outrow_end; ioutrow++)
            {
                for (ioutcol = outcol_start; ioutcol < outcol_end; ioutcol++)
                {
                    
                    int32_t accumulator = 0;
                    const Weight_t * weight_start = weights;
                    const Weight_t * image_start = image +  (ioutrow * num_image_cols) + ioutcol;
                    
                    for (iimage = 0; iimage < num_images; iimage++) {
                        
                        //element by element multiply of one matrix against another
                        //summing as you go.
                        
                        const Weight_t * image_row = image_start;
                        const Weight_t * weight_row = weight_start;
                        
                        for (j = 0; j < num_weights_rows; j++) {
                            
                            accumulator += accumulate(num_weights_cols,image_row,weight_row);
                            
                            /*
                            // ***** TODO optimize this right here *****
                            for (i = 0; i < num_weights_cols; i++) {
                                //image format is Q7 + Q_input_scaling = Q7 + QI
                                //weight format is Q7 + Q_weight_scaling = Q7 + QW
                                //the resulting format of the multiply is Q14 + QI + QW
                                accumulator += image_row[i] * weight_row[i];
                            }
                             */
                            
                            weight_row += num_weights_cols;
                            image_row += num_image_cols;
                        }
                        
                        //traverse to next slice for weights and image
                        weight_start += weight_size;
                        image_start += image_size;
                    }
                    
                    max_pool[ioutrow % pool_dims[0]][ioutcol % pool_dims[1]] = accumulator;
                }
                
            }
            
            
            //find max in pool
            temp32 = INT32_MIN;
            for (j = 0; j < pool_dims[0]; j++) {
                for (i = 0; i < pool_dims[1]; i++) {
                    if (temp32 < max_pool[j][i]) {
                        temp32 = max_pool[j][i];
                    }
                }
            }
            
            
            //dropout
            temp64 = temp32 * dropout_weight;
            temp64 >>= QFIXEDPOINT;
            
            //compensate for weight scaling
            //QBD = QW + QI - QB
            bias_scaling_diff = weight_scaling + input_scaling - bias_scaling;
            
            //Q7 + QB
            bias32 = bias;
            
            //Q14 + QB
            bias32 <<= QFIXEDPOINT;
            
            //convert bias to Q14 + QW + QI  from Q14 + QB
            //by scaling by QBD
            if (bias_scaling_diff > 0) {
                //bias is bigger!
                bias32 <<= bias_scaling_diff;
            }
            else {
                bias32 >>= -bias_scaling_diff;
            }
            
            
            //add bias
            //results in Q14 + QI + QW
            temp64 += bias32;
            
            //descale weight scaling
            //results in Q14 + QI
            temp64 >>= weight_scaling;
            
            //ADD ROUNDING
            temp64 += (1 << (QFIXEDPOINT - 1));
            
            //convert from Q14 to Q7
            temp64 >>= QFIXEDPOINT;
            
            
            //sanity check
            if (temp64 > INT32_MAX) {
                temp64 = INT32_MAX;
            }
            
            if (temp64 < INT32_MIN) {
                temp64 = INT32_MIN;
            }
            
            temp32 = temp64;
            temp32 >>= *descale;
            
            //descaling madness
            delta_descale = tiny_tensor_get_descaling(temp32);
            
            if (delta_descale) {
                temp32 >>= delta_descale; //update current temp value
                *descale += delta_descale; //update descale
                
                //backtrack -- right shift all previous by delta_scale
                //so fucking inefficient.
                for (p = out_begin; p < pout; p++) {
                    *p >>= delta_descale;
                }
            }
            
            if (temp32 > MAX_WEIGHT) {
                temp32 = MAX_WEIGHT;
            }
            
            if (temp32 < -MAX_WEIGHT) {
                temp32 = -MAX_WEIGHT;
            }
            
            *pout = (Weight_t)temp32;
            
            pout++;
        }
        
    }
    
    return  temp_scale - *descale;
}


static void eval_conv2d_direct(const void * context,void * layer_state,Tensor_t * out,const Tensor_t * in,ELayer_t prev_layer_type) {
    const ConvLayer2D_t * layer = (ConvLayer2D_t *)context;
    
    uint32_t iout;
    uint32_t i;

    int8_t descale = 0;
    int8_t total_scale;
    const uint32_t num_out_images = layer->weights->dims[0];
    const uint32_t num_images = layer->weights->dims[1];
    const uint32_t num_weights_rows = layer->weights->dims[2];
    const uint32_t num_weights_cols = layer->weights->dims[3];
    const uint32_t num_image_rows = in->dims[2];
    const uint32_t num_image_cols = in->dims[3];
    
    const Weight_t * weight_start = layer->weights->x;
    const uint32_t weight_filter_size = layer->weights->dims[1] * layer->weights->dims[2] * layer->weights->dims[3];
    
    const Weight_t * const image_start = in->x;    
    const Weight_t * bias = layer->biases->x;
    Weight_t * const out_begin = out->x;
    Weight_t * out_start = out->x;
    const uint32_t out_image_size = layer->output_dims[3] * layer->output_dims[2];
    Weight_t * pout = out->x;
    int8_t out_scale;
    Weight_t temp_weight;
    assert(layer->weights->dims[1] == in->dims[1]);
    
    for (i = 0; i < TENSOR_DIM; i++) {
        assert(in->dims[i] == layer->input_dims[i]);
    }
    
    //make sure output tensor is ready for this
    for (i = 0; i < TENSOR_DIM; i++) {
        assert(out->dims[i] == layer->output_dims[i]);
    }

    // each of M filters is a 3D tensor (multiple "images") + bias weight
    // each filter has dimensions of 1 x N x P x Q, where P x Q is the filter image size
    // thus the filter, i.e. the weights will have dimensions of M x N x P x Q
    // the biases will have dimensions of M x 1 x 1 x 1
    //
    // there are N images, of size U x V
    // thus dims of the input are 1 x N x U x V
    //
    // and dims of the output are
    // 1 x M x ((U - P + 1)) / pool_y   x    (V - Q + 1)  / pool_x
    //
    // so the idea is to build output images
    // from each filter
    for (iout = 0; iout < num_out_images; iout++) {
        
        total_scale = tinytensor_convolve3d_direct_maxpooling(&descale,
                                                out_begin,
                                                out_start,
                                                layer->max_pool_dims,
                                                weight_start,
                                                layer->weights->scale,
                                                image_start,
                                                in->scale,
                                                *bias,
                                                layer->biases->scale,
                                                num_weights_rows,
                                                num_weights_cols,
                                                num_image_rows ,
                                                num_image_cols,
                                                num_images,
                                                layer->incoming_dropout);
        
        
        bias += 1;
        out_start += out_image_size;
        weight_start += weight_filter_size;
    }
    
    //apply activations
    out_scale = 0;
    for (i = 0; i < out_image_size * num_out_images; i++) {
        layer->activation(&temp_weight,&out_scale,*pout,total_scale);
        *pout = temp_weight;
        pout++;
    }
    
    out->scale = out_scale;
/*
    int32_t max = 0;
    for (i = 0; i < out_len; i++) {
        if (abs(out->x[i]) > abs(max)) {
            max = out->x[i];
        }
    }
    
    //printf("max=%d\t\ts=%d\n",max,out->scale);
    printf("%d\t",max); fflush(0);

    */
    
}

ConstLayer_t tinytensor_create_conv_layer(const ConvLayer2D_t * static_def) {
    ConstLayer_t layer = {eval_conv2d_direct,get_conv2d_output_size,conv_layer,static_def,NULL,NULL};
    return layer;
}



