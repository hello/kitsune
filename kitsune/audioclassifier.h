#ifndef _AUDIOCLASSIFIER_H_
#define _AUDIOCLASSIFIER_H_

#include <pb.h>
#include "audio_types.h"
#include "machinelearning/machine_learning_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void AudioClassifier_Init(RecordAudioCallback_t recordfunc,void * buffer, uint32_t buf_size_in_bytes);

void AudioClassifier_DeserializeClassifier(pb_istream_t * stream, void * data);
    
void AudioClassifier_DataCallback(const AudioFeatures_t * pfeats);

uint32_t AudioClassifier_EncodeAudioFeatures(pb_ostream_t * stream,const void * encode_data);

void AudioClassifier_ResetStorageBuffer(void);

    
#ifdef __cplusplus
}
#endif

    
    
#endif //#ifndef _AUDIOCLASSIFIER_H_

