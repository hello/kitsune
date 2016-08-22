
#include "pn.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t state;
    uint16_t mask;
    uint32_t len;
} PNSequence_t;

const int8_t k_lookup[256] = {-8,-6,-6,-4,-6,-4,-4,-2,-6,-4,-4,-2,-4,-2,-2,0,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,-6,-4,-4,-2,-4,-2,-2,0,-4,-2,-2,0,-2,0,0,2,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,-4,-2,-2,0,-2,0,0,2,-2,0,0,2,0,2,2,4,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,-2,0,0,2,0,2,2,4,0,2,2,4,2,4,4,6,0,2,2,4,2,4,4,6,2,4,4,6,4,6,6,8};

#define MASK_16 (0xd008u)
#define MASK_9 (0x0110u)

#define PN_LEN_9           ((1<<9) - 1)
#define PN_LEN_16          ((1<<16) - 1)


static PNSequence_t _data;


void pn_init_with_state(uint16_t init_state, uint16_t mask, uint32_t len) {
    memset(&_data,0,sizeof(_data));
    _data.state = init_state;
    _data.mask = mask;
    _data.len = len;
}

void pn_init_with_mask_9() {
    pn_init_with_state(0xABCD,MASK_9,PN_LEN_9);
}

void pn_init_with_mask_16() {
    pn_init_with_state(0xABCD,MASK_16,PN_LEN_16);
}

uint32_t pn_get_length() {
    return _data.len;
}

uint8_t pn_get_next_bit() {
    const uint8_t lsb = (uint8_t)_data.state & 1;   /* Get LSB (i.e., the output bit). */
    _data.state >>= 1;
    _data.state ^= (-lsb) & _data.mask;
    return lsb;
}

int32_t pn_correlate_with_xor(const uint16_t * v1, const uint16_t * v2, uint32_t len) {
    int i;
    int32_t sum = 0;
    for (i = 0;i < len; i++) {
        const uint16_t res =  v1[i] ^ v2[i];
        const uint8_t b1 = (res & 0xFF00) >> 8;
        const uint8_t b2 = res & 0x00FF;
        sum += k_lookup[b1] + k_lookup[b2];
    }
    return sum;
}

#if 0

int main(void) {
#define LEN ((1 << 16) - 1)
    static const uint16_t mask = 0xd008u;
    int8_t seq1[LEN];
    int8_t seq2[LEN];
    uint16_t words1[1 << 12];
    uint16_t words2[1<< 12];
    
    memset(words1,0,sizeof(words1));
    memset(words2,0,sizeof(words2));

    uint16_t start_state = 0xACE1u;  /* Any nonzero start state will work. */
    uint16_t lfsr = start_state;
    int i = 0;
    int iword = 0;
    do
    {
        const unsigned lsb = lfsr & 1;   /* Get LSB (i.e., the output bit). */
        seq1[i] = lsb > 0 ? 1 : -1;
        if ( (i & 0x0F) == 0 && i > 0) {
            words1[iword] = lfsr;
            iword++;
        }
        
        
        
        lfsr >>= 1;                /* Shift register */
        lfsr ^= (-lsb) & mask;  /* If the output bit is 1, apply toggle mask.
                                    * The value has 1 at bits corresponding
                                * to taps, 0 elsewhere. */
        i++;
        
       
        
    } while (lfsr != start_state);
    
    printf("count1 = %d\n",i);
    start_state = 0xACEFu;
  //  start_state = 0xABCDu;
    lfsr = start_state;
    i = 0;
    iword = 0;

    do
    {
        const unsigned lsb = lfsr & 1;   /* Get LSB (i.e., the output bit). */
        seq2[i] = lsb > 0 ? 1 : -1;
        if ( (i & 0x0F) == 0 && i > 0) {
            words2[iword] = lfsr;
            iword++;
        }
        lfsr >>= 1;
        lfsr ^= (-lsb) & mask;
        i++;
        
        
        
    } while (lfsr != start_state);
    printf("count2 = %d\n",i);

    
    int32_t sum = 0;
    for (i = 0;  i < LEN ; i++) {
        sum += seq1[i] * seq2[i];
    }
    
    int32_t sum2 = 0;
    for (i = 0;i < 4096; i++) {
        const uint16_t res =  words1[i] ^ words2[i];
        const uint8_t b1 = (res & 0xFF00) >> 8;
        const uint8_t b2 = res & 0x00FF;
        sum2 += k_lookup[b1] + k_lookup[b2];
        
        int foo = 3;
        foo++;
        
    }
    
    printf("%d\n",sum);
    printf("%d\n",sum2);

    return 0;
}
#endif
