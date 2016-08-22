#include "upconvert.h"

//   fc * cycles_per_bit  = num_cycles
//   bpsk = -1 to 1 in complex baseband
//

// N * Fc / Fs = some whole number
//basically, this means that in cos_vec, how many cycles of the center frequency are there.
// Tc = 1.0 / Fc
// Tp = Tc  * N
// this defines how fast you can modulate
#define NUM_CYCLES_PER_PERIOD (11)

/*
const int16_t cos_vec[] = {32767,-31650,28377,-23169,16383,-8480,0,8480,-16383,23169,-28377,31650,-32767,31650,-28377,23169,-16383,8480,0,-8480,16383,-23169,28377,-31650};
*/

const int16_t sin_vec[] = {0,1579,-4085,7522,-11666,16025,-19874,22365,-22722,20447,-15498,8363,0,-8363,15498,-20447,22722,-22365,19874,-16025,11666,-7522,4085,-1579};



uint32_t upconvert_bits_bpsk(int16_t * samples, const uint8_t * bits, const uint32_t bitslen, const uint32_t samplebuflen) {
    uint32_t ibit;
    uint32_t isample = 0;
    uint32_t icycle;
    const uint16_t cycle_len = sizeof(sin_vec) / sizeof(typeof(sin_vec[0]));
    
    for (ibit = 0; ibit < bitslen; ibit++) {
        
        if (isample >= samplebuflen) {
            //TODO log error or something
            return isample;
        }
        
        if (bits[ibit] == 0) {
            for (icycle = 0; icycle < cycle_len; icycle++) {
                samples[isample++] = -sin_vec[icycle];
            }
        }
        else {
            for (icycle = 0; icycle < cycle_len; icycle++) {
                samples[isample++] = sin_vec[icycle];
            }
        }
        
    }
    
    return isample;
}