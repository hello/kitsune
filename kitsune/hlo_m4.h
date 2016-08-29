#ifndef HLO_M4
#define HLO_M4

#include "stdint.h"

__attribute__((section(".ramcode")))
inline int32_t hlo_asm_accumulate( int32_t a, const int16_t* w1,const int16_t* w2);

#endif
