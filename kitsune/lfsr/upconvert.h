#ifndef _UPCONVERT_H_
#define _UPCONVERT_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    
//returns number of samples written
uint32_t upconvert_bits_bpsk(int16_t * samples, const uint8_t * bits, const uint32_t bitslen, const uint32_t samplebuflen);

#ifdef __cplusplus
}
#endif


#endif
