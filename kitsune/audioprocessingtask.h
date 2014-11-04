#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"


typedef enum {
	processingOn,
	processingOff
} EAudioProcessingCommand_t;





void AudioProcessingTask_AddFeaturesToQueue(const AudioFeatures_t * feats);

//turn it on or off via a message
void AudioProcessingTask_SetControl(EAudioProcessingCommand_t cmd,NotificationCallback_t onFinished, void * context);
void AudioProcessingTask_Thread(void * data);

void AudioProcessingTask_FreeBuffers(void);
void AudioProcessingTask_AllocBuffers(void);


#endif //_AUDIOPROCESSINGTASK_H_
