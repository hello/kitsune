#ifndef _AUDIOCAPTURETASK_H_
#define _AUDIOCAPTURETASK_H_

#include <stdint.h>
#include "audio_types.h"

typedef enum {
	eAudioCaptureTurnOn,
	eAudioCaptureTurnOff,
	eAudioSaveToDisk,
	eAudioPlaybackStart,
	eAudioPlaybackStop
} EAudioCommand_t;

typedef struct {
	char file[64];
	int32_t volume;
	uint32_t durationInSeconds;

	NotificationCallback_t onFinished;
	void * context;

} AudioPlaybackDesc_t;

typedef struct {
	uint32_t captureduration;
} AudioCaptureDesc_t ;

typedef struct {
	EAudioCommand_t command;

	union {
		AudioCaptureDesc_t capturedesc;
		AudioPlaybackDesc_t playbackdesc;
	} message;

} AudioMessage_t;

void AudioTask_Thread(void * data);

void AudioTask_AddMessageToQueue(const AudioMessage_t * message);

void AudioTask_StartPlayback(const AudioPlaybackDesc_t * desc);

void AudioTask_StopPlayback(void);

void AudioTask_StopCapture(void);

void AudioTask_StartCapture(void);

#endif //_AUDIOCAPTURETASK_H_
