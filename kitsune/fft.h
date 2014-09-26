#ifndef FFT_H
#define FFT_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MEL_SCALE_SIZE (16)
    
int32_t fft(int16_t fr[], int16_t fi[], int32_t m);
int32_t fftr(int16_t f[], int32_t m); //this does not work, do not use it.

    
uint8_t bitlog(uint32_t n);
uint32_t bitexp(uint16_t n);

int16_t fxd_sin( uint16_t x );
uint32_t fxd_sqrt (uint32_t n);
uint32_t fxd_sqrt_q10(uint32_t x);

void abs_fft(uint16_t psd[], const int16_t fr[],const int16_t fi[],const int16_t len);
void logpsd(int16_t * logTotalEnergy,int16_t psd[],const int16_t fr[],const int16_t fi[],uint8_t log2scaleOfRawSignal,const uint16_t numelemenets);

void dct(int16_t fr[],int16_t fi[],const int16_t ndct);

    
int16_t FixedPointLog2Q10(uint64_t x);
uint8_t CountHighestMsb(uint64_t x);
uint32_t FixedPointExp2Q10(const int16_t x);



#ifdef __cplusplus
}
#endif
    
#endif
