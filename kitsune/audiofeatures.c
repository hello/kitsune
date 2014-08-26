#include "audiofeatures.h"
#include "fft.h"
#include <string.h>
#include <stdlib.h>     /* abs */

#define MEL_BUF_SIZE_2N (8)
#define MEL_BUF_SIZE (1 << MEL_BUF_SIZE_2N)
#define MEL_BUF_MASK (MEL_BUF_SIZE - 1)


/*--------------------------------
 *   Types
 *--------------------------------*/

typedef struct {
    int8_t melbuf[MEL_SCALE_ROUNDED_UP][MEL_BUF_SIZE]; //8 x 256 --> 2K
    int16_t melaccumulator[MEL_SCALE_ROUNDED_UP];
    uint16_t idx;
    uint8_t isBufferFull;
    
} MelFeatures_t;

/*--------------------------------
 *   Static Memory Declarations
 *--------------------------------*/
static MelFeatures_t _data;


/*--------------------------------
 *   Functions
 *--------------------------------*/
void AudioFeatures_Init() {
    memset(&_data,0,sizeof(_data));
}

uint8_t AudioFeatures_MelAveraging(uint32_t idx, int8_t x,int8_t * buf, int16_t * accumulator) {
    /* accumulate */
    uint16_t a = *accumulator;
    /* add new measurement to accumulator */
    a += x;
    
    /*  remove oldest measurement from accumulator */
    a -= buf[idx];
    
    /*  store new measurement */
    buf[idx] = x;
    
    *accumulator = a;
    
    return (uint8_t) (a >> MEL_BUF_SIZE_2N);
}


uint8_t AudioFeatures_Extract(int16_t * logmfcc, const int16_t samples[],int16_t nfftsize) {
    //enjoy this nice large stack.
    //this can all go away if we get fftr to work, and do the
    int16_t fr[AUDIO_FFT_SIZE]; //2K
    int16_t fi[AUDIO_FFT_SIZE] = {0}; //2K
    int16_t psd[AUDIO_FFT_SIZE >> 1]; //1K
    uint8_t mel[MEL_SCALE_SIZE]; //inconsiquential
    uint16_t i;
    int16_t temp16;
    
    
    memcpy(fr,samples,sizeof(fr));
    
    /* Get FFT */
    fft(fr,fi, AUDIO_FFT_SIZE_2N);
    
    /* Get "PSD" */
    abs_fft(psd, samples, fi, AUDIO_FFT_SIZE >> 1);
    
    /* Get Mel */
    mel_freq(mel, psd,AUDIO_FFT_SIZE_2N,EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE);
    
    /*  get dct of mel */
    for (i = 0; i < MEL_SCALE_ROUNDED_UP; i++) {
        fr[i] = mel[i];
        fi[i] = 0;
    }
    
    /* fr will contain the dct */
    fft(fr,fi,MEL_SCALE_ROUNDED_UP_2N);
    
    for (i = 0; i < MEL_SCALE_ROUNDED_UP; i++) {
        //signed bitlog!
        temp16 = bitlog(abs(fr[i]));
        if (fr[i] < 0) {
            temp16 = -temp16;
        }
        
        fr[i] = temp16;
    }
    
    /* Moving Average */
    for (i = 0; i < MEL_SCALE_ROUNDED_UP; i++) {
        temp16 = fr[i];
        
        if (temp16 > 127) {
            temp16 = 127;
        }
        
        if (temp16 <= -128) {
            temp16 = -128;
        }
        
        
        AudioFeatures_MelAveraging(_data.idx,(int8_t)temp16,_data.melbuf[i],&_data.melaccumulator[i]);
        
        logmfcc[i] = _data.melaccumulator[i];
    }
    
    _data.idx++;
    
    if (_data.idx >= MEL_BUF_SIZE) {
        _data.isBufferFull = 1;
    }
    
    _data.idx &= MEL_BUF_MASK;
    
    return _data.isBufferFull;
}
