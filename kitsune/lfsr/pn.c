
#include "pn.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t state;
    uint16_t mask;
    uint32_t len;
} PNSequence_t;

//lookup table for number for sum of bits where 0 ---> -1, and 1 ---> 1
const int8_t k_lookup[256] = {-8,-6,-6,-4,-6,-4,-4,-2,-6,-4,-4,-2,-4,-2,-2,0,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,0,2,2,4,2,4,4,6,2,4,4,6,4,6,6,8};

/*straight from wikipedia, ha.
  https://en.wikipedia.org/wiki/Linear-feedback_shift_register
*/
#define MASK_9 (0x0110u)
#define MASK_10 (0x0240u)
#define MASK_12 (0x0E08u)
#define MASK_14 (0x3802u)
#define MASK_16 (0xd008u)


static PNSequence_t _data;


void pn_init_with_state(uint16_t init_state, uint16_t mask, uint32_t len) {
    memset(&_data,0,sizeof(_data));
    _data.state = init_state;
    _data.mask = mask;
    _data.len = len;
}


void pn_init_with_mask_9(void) {
    pn_init_with_state(0xABCD,MASK_9,PN_LEN_9);
}

void pn_init_with_mask_12(void) {
    pn_init_with_state(0xABCD,MASK_12,PN_LEN_12);
}

void pn_init_with_mask_14(void) {
    pn_init_with_state(0xABCD,MASK_14,PN_LEN_14);
}

uint32_t pn_get_length() {
    return _data.len;
}

inline uint8_t pn_get_next_bit() {
    const uint8_t lsb = (uint8_t)_data.state & 1;   /* Get LSB (i.e., the output bit). */
    _data.state >>= 1;
    _data.state ^= (-lsb) & _data.mask;
    return lsb;
}

//stateful
void pn_correlate_4x(uint32_t x, int16_t sums[4][8],uint8_t * the_byte) {
    int16_t i;
    uint8_t b;
    uint32_t x2;
    
    for (i = 0; i < 8; i++) {
        *the_byte <<= 1;
        *the_byte |= pn_get_next_bit();
        
        b = *the_byte;
        
        x2 = (b << 24) | (b << 16) | (b << 8) | (b);
        
        x2 ^= x;
        
        sums[0][i] += k_lookup[(x2 & 0x000000FF) >> 0];
        sums[1][i] += k_lookup[(x2 & 0x0000FF00) >> 8];
        sums[2][i] += k_lookup[(x2 & 0x00FF0000) >> 16];
        sums[3][i] += k_lookup[(x2 & 0xFF000000) >> 24];
    }

    
}



