#ifndef _AUDIOCAPTURETASK_H_
#define _AUDIOCAPTURETASK_H_

#include <stdint.h>
#include <stdbool.h>
#include "audio_types.h"


typedef enum {
	eAudioCaptureTurnOn,
	eAudioCaptureTurnOff,
	eAudioSaveToDisk,
	eAudioPlaybackStart,
	eAudioPlaybackStop,
	eAudioCaptureOctogram
} EAudioCommand_t;

typedef struct {
	char file[64];
	int32_t volume;
	int32_t durationInSeconds;
	uint32_t rate;

	NotificationCallback_t onFinished;
	void * context;

} AudioPlaybackDesc_t;


typedef struct {
	uint32_t analysisduration;

	NotificationCallback_t onFinished;
	void * context;
	OctogramResult_t * result;

} AudioOctogramDesc_t;

typedef struct {
	EAudioCommand_t command;

	union {
		AudioCaptureDesc_t capturedesc;
		AudioPlaybackDesc_t playbackdesc;
		AudioOctogramDesc_t octogramdesc;
	} message;

} AudioMessage_t;

void AudioTask_Thread(void * data);

void AudioTask_AddMessageToQueue(const AudioMessage_t * message);

void AudioTask_StartPlayback(const AudioPlaybackDesc_t * desc);

void AudioTask_StopPlayback(void);

void AudioTask_StopCapture(void);

void AudioTask_StartCapture(uint32_t rate);

void AudioTask_DumpOncePerMinuteStats(AudioOncePerMinuteData_t * pdata);

void AudioTask_AddPillId(const char * pill_id, bool reset);
void AudioTask_SetMotionFromPill(const char * pill_id);

#endif //_AUDIOCAPTURETASK_H_
