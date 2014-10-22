#ifndef _AUDIOCLASSIFIER_H_
#define _AUDIOCLASSIFIER_H_

#include <pb.h>
#include "audio_types.h"
#include "machinelearning/machine_learning_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void AudioClassifier_Init(RecordAudioCallback_t recordfunc,MutexCallback_t lockfunc, MutexCallback_t unlockfunc) ;

void AudioClassifier_DeserializeClassifier(pb_istream_t * stream);
    
void AudioClassifier_DataCallback(const AudioFeatures_t * pfeats);

uint32_t AudioClassifier_GetSerializedBuffer(pb_ostream_t * stream,const char * macbytes, uint32_t unix_time,const char * tags, const char * source);

    
#ifdef __cplusplus
}
#endif

    
    
#endif //#ifndef _AUDIOCLASSIFIER_H_

