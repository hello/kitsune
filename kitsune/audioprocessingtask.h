#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"
/****************************
 * AUDIO FEATURE MESSAGE
 ****************************/
typedef struct {
	Segment_t seg;
	int16_t feat[NUM_AUDIO_FEATURES];
} AudioFeatureMessage_t;



void AudioProcessingTask_Init(void);

void AudioProcessingTask_AddMessageToQueue(const AudioFeatureMessage_t * message);

void AudioProcessingTask_Thread(void * data);


#endif //_AUDIOPROCESSINGTASK_H_
