#ifndef _TINYTENSOR_MATH_H_
#define _TINYTENSOR_MATH_H_

#include "tinytensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QFIXEDPOINT (7)
#define QFIXEDPOINT_INT16 (15)
#define TOFIX(x)\
        (Weight_t)(x * (1 << QFIXEDPOINT))
    
#define TOFLT(x)\
        ( ((float)x) / (float)(1 << QFIXEDPOINT))
    
#define MUL16(a,b)\
    ((int16_t)(((int32_t)(a * b)) >> QFIXEDPOINT_INT16))

/* INPUTS ARE EXPECTED TO BE IN Q7, JUST POTENTIALLY VERY LARGE IN MAGNITUDE */
void tinytensor_tanh(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale);
void tinytensor_sigmoid(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale);
void tinytensor_linear(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale);
void tinytensor_relu(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale);

    
void tinytensor_vec_softmax_in_place(Weight_t * xvec, uint32_t len, int8_t in_scale);

void tinytensor_descale(Weight_t * y, int8_t * out_scale, int32_t x, int8_t in_scale);
int8_t tiny_tensor_compare_scaled_numbers(const Weight_t x1, const int8_t scale1, const Weight_t x2, const int8_t scale2);
int8_t tiny_tensor_get_scaling(int32_t x);
int8_t tiny_tensor_get_descaling(int32_t x);


    
static inline int32_t accumulate(const uint32_t n, const Weight_t * in1, const Weight_t * in2) {
    int32_t accumulator = 0;
    int16_t n_this_iter;
    int16_t nloop = n;
    nloop = n;
    
    while (nloop > 0) {
        
        n_this_iter = nloop > 8 ? 8 : nloop;
        nloop -= 8;
        
        //pray for jump table
        switch(n_this_iter) {
            case 1:
            {
                accumulator += in1[0] * in2[0];
                
                break;
            }
                
            case 2:
            {
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                
                break;
            }
                
                
            case 3:
            {
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                accumulator += in1[2] * in2[2];
                
                break;
            }
                
            case 4:
            {
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                accumulator += in1[2] * in2[2];
                accumulator += in1[3] * in2[3];
                
                break;
            }
                
            case 5:
            {
                //some SSE stuff that could help
                // _mm_loadu_si128
                // temp_products = _mm_mullo_epi32(temp_1, temp_2);
                //Sum temp_sum
                //temp_sum = _mm_add_epi32(temp_sum, temp_products);
                
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                accumulator += in1[2] * in2[2];
                accumulator += in1[3] * in2[3];
                accumulator += in1[4] * in2[4];
                
                break;
            }
                
            case 6:
            {
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                accumulator += in1[2] * in2[2];
                accumulator += in1[3] * in2[3];
                accumulator += in1[4] * in2[4];
                accumulator += in1[5] * in2[5];
                
                break;
            }
                
            case 7:
            {
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                accumulator += in1[2] * in2[2];
                accumulator += in1[3] * in2[3];
                accumulator += in1[4] * in2[4];
                accumulator += in1[5] * in2[5];
                accumulator += in1[6] * in2[6];
                
                break;
            }
                
            case 8:
            {
                accumulator += in1[0] * in2[0];
                accumulator += in1[1] * in2[1];
                accumulator += in1[2] * in2[2];
                accumulator += in1[3] * in2[3];
                accumulator += in1[4] * in2[4];
                accumulator += in1[5] * in2[5];
                accumulator += in1[6] * in2[6];
                accumulator += in1[7] * in2[7];
                break;
            }
        }
        
        in1 += 8;
        in2 += 8;
    }
    
    return accumulator;
    
}

    
#ifdef __cplusplus
}
#endif

#endif
