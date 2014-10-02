#ifndef _AUDIOFEATURES_H_
#define _AUDIOFEATURES_H_

#include <stdint.h>

#include "audio_types.h"



#ifdef __cplusplus
extern "C" {
#endif

    


/*  exported for your enjoyment -- use these! */
void AudioFeatures_Init(SegmentAndFeatureCallback_t fpCallback);
void AudioFeatures_SetAudioData(const int16_t buf[],int16_t nfftsize, int64_t samplecount);


#ifdef __cplusplus
}
#endif
    
#endif //_AUDIOFEATURES_H_
