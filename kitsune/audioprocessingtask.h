#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"

#define AUDIOPROCESSING_FLAG

typedef enum {
	processingOn,
	processingOff,
	rawUploadsOn,
	rawUploadsOff,
	featureUploadsOn,
	featureUploadsOff
} EAudioProcessingCommand_t;


//how we hand off features
void AudioProcessingTask_AddFeaturesToQueue(const AudioFeatures_t * feats);

//turn processon on  or off via a message
//turning it off frees the storage buffers
//turn it on allocates the storage buffers
void AudioProcessingTask_SetControl(EAudioProcessingCommand_t cmd, NotificationCallback_t onFinished, void * context);

//our thread function -- loops forever
void AudioProcessingTask_Thread(void * data);


#endif //_AUDIOPROCESSINGTASK_H_
