#ifndef _AUDIOFEATURES_H_
#define _AUDIOFEATURES_H_

#include <stdint.h>


#define AUDIO_FFT_SIZE_2N (10)
#define AUDIO_FFT_SIZE (1 << AUDIO_FFT_SIZE_2N)
#define EXPECTED_AUDIO_SAMPLE_RATE_HZ (44100)

#define MEL_SCALE_ROUNDED_UP_2N (4)
#define MEL_SCALE_ROUNDED_UP (1 << MEL_SCALE_ROUNDED_UP_2N)

#ifdef __cplusplus
extern "C" {
#endif


void AudioFeatures_Init();

uint8_t AudioFeatures_MelAveraging(uint32_t idx, int8_t x,int8_t * buf, int16_t * accumulator);

uint8_t AudioFeatures_Extract(int16_t * logmfcc, const int16_t buf[],int16_t nfftsize);


#ifdef __cplusplus
}
#endif
    
#endif //_AUDIOFEATURES_H_