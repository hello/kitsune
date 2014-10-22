#ifndef _AUDIOPROCESSINGTASK_H_
#define _AUDIOPROCESSINGTASK_H_

#include <stdint.h>
#include "audio_types.h"
/****************************
 * AUDIO FEATURE MESSAGE
 ****************************/

void AudioProcessingTask_Init(void);

void AudioProcessingTask_AddMessageToQueue(const AudioFeatures_t * message);

void AudioProcessingTask_Thread(void * data);


#endif //_AUDIOPROCESSINGTASK_H_
