#ifndef _AUDIOCLASSIFIER_H_
#define _AUDIOCLASSIFIER_H_


#include "audio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void AudioClassifier_Init(SegmentAndFeatureCallback_t fpNovelFeatureCallback);

void AudioClassifier_SegmentCallback(const int16_t * feats, const Segment_t * pSegment);
 
#ifdef __cplusplus
}
#endif


#endif

