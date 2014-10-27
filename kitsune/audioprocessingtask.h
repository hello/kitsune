#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"

typedef enum {
	eFeats,
	eTimeToUpload,
} EAudioProcessingTaskMessage_t;

typedef struct {
	EAudioProcessingTaskMessage_t type;

	union {
		AudioFeatures_t feats;
		int empty;
	} payload;


} AudioProcessingTaskMessage_t;


void AudioProcessingTask_AddFeaturesToQueue(const AudioProcessingTaskMessage_t * message);

void AudioProcessingTask_Thread(void * data);


#endif //_AUDIOPROCESSINGTASK_H_
