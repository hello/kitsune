#ifndef _AUDIOFEATURES_H_
#define _AUDIOFEATURES_H_

#include <stdint.h>


#define AUDIO_FFT_SIZE_2N (10)
#define AUDIO_FFT_SIZE (1 << AUDIO_FFT_SIZE_2N)
#define EXPECTED_AUDIO_SAMPLE_RATE_HZ (44100)

#define NUM_MFCC_FEATURES_2N (4)
#define NUM_MFCC_FEATURES (1 << NUM_MFCC_FEATURES_2N)

#ifdef __cplusplus
extern "C" {
#endif

    
typedef enum {
    segmentPacket,
    segmentSteadyState
} ESegmentType_t;
    
typedef struct {
    int64_t t1;
    int64_t t2;
    ESegmentType_t type;
    
} Segment_t;
    
typedef void (*SegmentAndFeatureCallback_t)(const int32_t * mfccfeats, const Segment_t * pSegment);

/*  exported for your enjoyment -- use these! */
void AudioFeatures_Init(SegmentAndFeatureCallback_t fpCallback);
void AudioFeatures_SetAudioData(const int16_t buf[],int16_t nfftsize, int64_t samplecount);


#ifdef __cplusplus
}
#endif
    
#endif //_AUDIOFEATURES_H_