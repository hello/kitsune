
#ifndef _LFSR_H_
#define _LFSR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pn_correlate_4x(uint32_t x, int16_t sums[4][8],uint8_t * the_byte);
uint8_t pn_get_next_bit();
void pn_init_with_mask_9(void);
void pn_init_with_mask_12(void);
uint32_t pn_get_length();
    
#ifdef __cplusplus
}
#endif
    
#endif
