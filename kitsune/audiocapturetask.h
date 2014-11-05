#ifndef _AUDIOCAPTURETASK_H_
#define _AUDIOCAPTURETASK_H_

#include <stdint.h>

typedef void (*CommandCompleteNotification)(void);

typedef enum {
	eAudioCaptureCarryOnAsUsual,
	eAudioCaptureTurnOn,
	eAudioCaptureTurnOff,
	eAudioCaptureSaveToDisk,
	eAudioCaptureDisable,
	eAudioCaptureEnable

} EAudioCaptureCommand_t;

typedef struct {
	EAudioCaptureCommand_t command;
	uint32_t captureduration;
	CommandCompleteNotification fpCommandComplete;

} AudioCaptureMessage_t;

void AudioCaptureTask_Thread(void * data);

void AudioCaptureTask_AddMessageToQueue(const AudioCaptureMessage_t * message);

void AudioCaptureTask_Enable(void);

void AudioCaptureTask_Disable(void);


#endif //_AUDIOCAPTURETASK_H_
