#ifndef FFT_H
#define FFT_H

#ifdef __cplusplus
extern "C" {
#endif

#define MEL_SCALE_SIZE (15)

    
int fft(short fr[], short fi[], int m);
int fftr(short f[], int m); //this does not work, do not use it.

void mel_freq(short mel[],const short f[], int n, int b );

short fxd_sin( unsigned short x );
unsigned int fxd_sqrt (unsigned int n); //untested

void abs_fft(short psd[], const short fr[],const short fi[], short nfft);

short FixedPointLog2Q8(unsigned int x);
short CountHighestMsb(unsigned int x);


#ifdef __cplusplus
}
#endif
    
#endif
