#ifndef FFT_H
#define FFT_H

#ifdef __cplusplus
extern "C" {
#endif

int fft(short fr[], short fi[], int m);
int fftr(short f[], int m);
void fix_window(short fr[], int n);
void psd(short f[], int m);
void mel_freq(short f[], int n, int b );

short bitlog(unsigned long n);
unsigned long bitexp(unsigned short n);
short fxd_sin( unsigned short x );
unsigned int fxd_sqrt (unsigned int n);

#ifdef __cplusplus
}
#endif
    
#endif
