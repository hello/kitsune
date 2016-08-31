#ifndef _AUDIOFEATURES_H_
#define _AUDIOFEATURES_H_

#include <stdint.h>

#include "audio_types.h"



#ifdef __cplusplus
extern "C" {
#endif

    


/*  exported for your enjoyment -- use these! */
void AudioFeatures_Init(AudioOncePerMinuteDataCallback_t fpOncePerMinuteCallback);
    
/*  Expects AUDIO_FFT_SIZE samples in samplebuf  */
void AudioFeatures_SetAudioData(const int16_t samples[],int64_t samplecount);

#ifdef __cplusplus
}
#endif
    
#endif //_AUDIOFEATURES_H_
