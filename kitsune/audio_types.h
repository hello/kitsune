#ifndef _AUDIOTYPES_H_
#define _AUDIOTYPES_H_

#include <stdint.h>

//magic number, in no particular units, just from observation
#define MIN_CLASSIFICATION_ENERGY (500)


#define AUDIO_FFT_SIZE_2N (8)
#define AUDIO_FFT_SIZE (1 << AUDIO_FFT_SIZE_2N)
#define EXPECTED_AUDIO_SAMPLE_RATE_HZ (16000)

#define SAMPLE_RATE_IN_HZ (EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE)
#define SAMPLE_PERIOD_IN_MILLISECONDS  (1000 / SAMPLE_RATE_IN_HZ)

#define NUM_AUDIO_FEATURES (16)

/*
 // use simplelink.h instead
#define TRUE (1)
#define FALSE (0)
*/




#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t psd_min_energy;
    
} AudioFeaturesOptions_t;
    
typedef struct {
    uint16_t min_energy_classification;
} AudioClassifierOptions_t;

typedef enum {
    segmentCoherent
} ESegmentType_t;

typedef struct {
    int64_t t1;
    int64_t t2;
    int32_t duration;
    ESegmentType_t type;
    
} Segment_t;

typedef struct {
    int16_t logenergy;
    int16_t logenergyOverBackroundNoise;
    int8_t feats4bit[NUM_AUDIO_FEATURES];
} AudioFeatures_t;
    
typedef struct {
    uint32_t durationInFrames;
    
} RecordAudioRequest_t;

typedef void (*SegmentAndFeatureCallback_t)(const int16_t * feats, const Segment_t * pSegment);
typedef void (*AudioFeatureCallback_t)(int64_t samplecount,const AudioFeatures_t * pfeats);
typedef void (*NotificationCallback_t)(void);
typedef void (*RecordAudioCallback_t)(const RecordAudioRequest_t * request);
typedef void (*MutexCallback_t)(void);

    
#ifdef __cplusplus
}
#endif


#endif
