#ifndef _AUDIOSIMILARITY_H_
#define _AUDIOSIMILARITY_H_

#include "audio_types.h"
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

void AudioClassifier_Init(uint8_t updateCountNoveltyThreshold,NotificationCallback_t novelDataCallback,MutexCallback_t fpLock, MutexCallback_t fpUnlock);

void AudioClassifier_ResetUpdateTime(void);
    
uint32_t AudioClassifier_GetSerializedBuffer(pb_ostream_t * stream,const char * macbytes, uint32_t unix_time,const char * tags, const char * source);

void AudioClassifier_DataCallback(AudioFeatures_t * feats);
 
#ifdef __cplusplus
}
#endif


#endif

