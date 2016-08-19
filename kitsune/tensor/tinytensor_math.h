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

#if 1
#include "hlo_m4.h"
#define accumulate hlo_asm_accumulate
#else

typedef union
{
    struct {
        uint32_t LSW; /* Least significant Word */
        uint32_t MSW; /* Most significant Word */
    }regs;
    uint64_t pair;
}RegPair_t;

#include "cmsis_ccs.h"
#include "arm_math.h"

__attribute__((section(".ramcode")))
static inline int32_t accumulate(int32_t n, int16_t* in1, int16_t* in2)
{
    RegPair_t ba,bb;
    int32_t accumulator = 0;

    uint32_t * aw = (uint32_t*)in1;
    uint32_t * bw = (uint32_t*)in2;
    while (n >= 8) {
        ba.pair = *__SIMD64(aw)++;
        bb.pair = *__SIMD64(bw)++;
        accumulator = __SMLAD(ba.regs.MSW, bb.regs.MSW, accumulator);
        accumulator = __SMLAD(ba.regs.LSW, bb.regs.LSW, accumulator);
        ba.pair = *__SIMD64(aw)++;
        bb.pair = *__SIMD64(bw)++;
        accumulator = __SMLAD(ba.regs.MSW, bb.regs.MSW, accumulator);
        accumulator = __SMLAD(ba.regs.LSW, bb.regs.LSW, accumulator);
        n -= 8;
    }
    while( n>=4 ) {
        ba.pair = *__SIMD64(aw)++;
        bb.pair = *__SIMD64(bw)++;
        accumulator = __SMLAD(ba.regs.MSW, bb.regs.MSW, accumulator);
        accumulator = __SMLAD(ba.regs.LSW, bb.regs.LSW, accumulator);
        n-=4;
    }
    while( n>=2 ) {
		accumulator = __SMLAD(*aw++, *bw++, accumulator);
        n-=2;
    }

    return accumulator;
}


#endif

#ifdef __cplusplus
}
#endif

#endif
