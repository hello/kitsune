#ifndef _AUDIOCAPTURETASK_H_
#define _AUDIOCAPTURETASK_H_

#include <stdint.h>

typedef enum {
	eAudioCaptureCarryOnAsUsual,
	eAudioCaptureTurnOn,
	eAudioCaptureTurnOff,
	eAudioCaptureSaveToDisk,

} EAudioCaptureCommand_t;

typedef struct {
	EAudioCaptureCommand_t command;
	uint32_t captureduration;


} AudioCaptureMessage_t;

void AudioCaptureTask_Init(void);

void AudioCaptureTask_Thread(void * data);

void AudioCaptureTask_AddMessageToQueue(const AudioCaptureMessage_t * message);


#endif //_AUDIOCAPTURETASK_H_
