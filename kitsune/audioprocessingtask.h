#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"




void AudioProcessingTask_AddFeaturesToQueue(const AudioFeatures_t * feats);

void AudioProcessingTask_Thread(void * data);

void AudioProcessingTask_FreeBuffers(void);
void AudioProcessingTask_AllocBuffers(void);


#endif //_AUDIOPROCESSINGTASK_H_
