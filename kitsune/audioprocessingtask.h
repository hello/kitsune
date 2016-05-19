#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"
#include "FreeRTOS.h"

#define AUDIOPROCESSING_FLAG

typedef enum {
	featureUploadsOn,
	featureUploadsOff
} EAudioProcessingCommand_t;


//how we hand off features
void AudioProcessingTask_AddFeaturesToQueue(const AudioFeatures_t * feats);

//turn processon on  or off via a message
//turning it off frees the storage buffers
//turn it on allocates the storage buffers
void AudioProcessingTask_SetControl(EAudioProcessingCommand_t cmd, TickType_t wait );

//our thread function -- loops forever
void AudioProcessingTask_Thread(void * data);


#endif //_AUDIOPROCESSINGTASK_H_
