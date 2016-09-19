#ifndef _TINYTENSORMATHDEFS_H_
#define _TINYTENSORMATHDEFS_H_

#include <stdint.h>

#define QFIXEDPOINT (12)
#define QFIXEDPOINT_INT16 (15)
#define TOFIX(x)\
        (Weight_t)(x * (1 << QFIXEDPOINT))

#define TOFIXQ(x,q)\
        ((int32_t) ((x) * (float)(1 << (q))))

#define TOFLT(x)\
        ( ((float)x) / (float)(1 << QFIXEDPOINT))

#define MUL16(a,b)\
    ((int16_t)(((int32_t)(a * b)) >> QFIXEDPOINT_INT16))

#endif //_TINYTENSORMATHDEFS_H_
