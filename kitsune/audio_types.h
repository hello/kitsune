#ifndef _AUDIOTYPES_H_
#define _AUDIOTYPES_H_

#include <stdint.h>
#include <simplelink.h>

#define AUDIO_FFT_SIZE_2N (10)
#define AUDIO_FFT_SIZE (1 << AUDIO_FFT_SIZE_2N)
#define EXPECTED_AUDIO_SAMPLE_RATE_HZ (44100)

#define NUM_AUDIO_FEATURES (16)

#define MAX_INT_16 (32767)

//purposely DO NOT MAKE THIS -32768
// abs(-32768) is 32768.  I can't represent this number with an int16 type!
#define MIN_INT_16 (-32767)
#define MIN_INT_32 (-2147483647)

#define MUL(a,b,q)\
((int16_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))

#define MUL_PRECISE_RESULT(a,b,q)\
((int32_t)((((int32_t)(a)) * ((int32_t)(b))) >> q))

#define TOFIX(x,q)\
((int32_t) ((x) * (float)(1 << (q))))



#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    segmentCoherent
} ESegmentType_t;

typedef struct {
    int64_t t1;
    int64_t t2;
    int32_t duration;
    ESegmentType_t type;
    
} Segment_t;

typedef void (*SegmentAndFeatureCallback_t)(const int16_t * feats, const Segment_t * pSegment);
typedef void (*NotificationCallback_t)(void);

typedef void (*MutexCallback_t)(void);

    
#ifdef __cplusplus
}
#endif


#endif
