#ifndef _HELLOMATH_H_
#define _HELLOMATH_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_INT_16 (32767)
    
    //purposely DO NOT MAKE THIS -32768
    // abs(-32768) is 32768.  I can't represent this number with an int16 type!
#define MIN_INT_16 (-32767)
#define MIN_INT_32 (-2147483647)
    
#define MUL(a,b,q)\
((int16_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))
    
#define MUL_PRECISE_RESULT(a,b,q)\
((int32_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))
    
uint32_t fxd_sqrt (uint32_t n);
    
uint32_t fxd_sqrt_q10(uint32_t x);
    
int32_t FixedPointLog2Q10(uint64_t x);
    
uint8_t CountHighestMsb(uint64_t x);

uint32_t FixedPointExp2Q10(const int16_t x);
    
int16_t cosvec16(const int16_t * vec1, const int16_t * vec2, uint8_t n);
    
int16_t cosvec8(const int8_t * vec1, const int8_t * vec2, uint8_t n);
    
uint8_t VecNormalize8(int8_t * vec, uint8_t n);
    
void Scale16VecTo8(int8_t * pscaled, const int16_t * vec,uint16_t n);

void MatMul(int16_t * out, const int16_t * a, const int16_t * b, uint8_t numrowsA,uint8_t numcolsA, uint8_t numcolsB,uint8_t q);

    

#ifdef __cplusplus
}
#endif

#endif
