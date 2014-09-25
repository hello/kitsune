#ifndef _AUDIOTYPES_H_
#define _AUDIOTYPES_H_


#define AUDIO_FFT_SIZE_2N (10)
#define AUDIO_FFT_SIZE (1 << AUDIO_FFT_SIZE_2N)
#define EXPECTED_AUDIO_SAMPLE_RATE_HZ (44100)

#define NUM_AUDIO_FEATURES (9)

#define NUM_FREQ_ENERGY_BINS (3)

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

typedef void (*SegmentAndFeatureCallback_t)(const int16_t * feats, const Segment_t * pSegment);

    
#ifdef __cplusplus
}
#endif


#endif
