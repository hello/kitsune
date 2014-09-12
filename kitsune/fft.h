#ifndef FFT_H
#define FFT_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MEL_SCALE_SIZE (16)
    
int32_t fft(int16_t fr[], int16_t fi[], int32_t m);
int32_t fftr(int16_t f[], int32_t m); //this does not work, do not use it.

void mel_freq(int16_t mel[],int16_t mel_denoised[], const int16_t mel_correction[],const int16_t fr[],const int16_t fi[],uint8_t log2scaleOfRawSignal);
    
uint8_t bitlog(uint32_t n);
uint32_t bitexp(uint16_t n);

int16_t fxd_sin( uint16_t x );
uint32_t fxd_sqrt (uint32_t n); //untested

void abs_fft(uint16_t psd[], const int16_t fr[],const int16_t fi[],const int16_t len);

int16_t FixedPointLog2Q10(uint64_t x);
uint8_t CountHighestMsb(uint64_t x);
uint32_t FixedPointExp2Q10(const int16_t x);



#ifdef __cplusplus
}
#endif
    
#endif
