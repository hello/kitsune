#ifndef FFT_H
#define FFT_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MEL_SCALE_SIZE (15)
    
int32_t fft(int16_t fr[], int16_t fi[], int32_t m);
int32_t fftr(int16_t f[], int32_t m); //this does not work, do not use it.

void mel_freq(uint8_t mel[],const int16_t f[], int32_t n, int32_t b );

uint8_t bitlog(uint32_t n);
uint32_t bitexp(uint16_t n);

int16_t fxd_sin( uint16_t x );
uint32_t fxd_sqrt (uint32_t n); //untested

void abs_fft(int16_t psd[], const int16_t fr[],const int16_t fi[],const int16_t len);

int16_t FixedPointLog2Q8(unsigned int x);
int16_t CountHighestMsb(unsigned int x);


#ifdef __cplusplus
}
#endif
    
#endif
