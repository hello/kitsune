#ifndef _AUDIOCAPTURETASK_H_
#define _AUDIOCAPTURETASK_H_

#include <stdint.h>
#include "audio_types.h"

#include "hlo_stream.h"
#include "hlo_pipe.h"

#include "codec_debug_config.h"

#define MAX_SOURCE_NAME_CHAR 	(68)

typedef enum {
	eAudioPlaybackStart,
	eAudioPlaybackStop,
	eAudioCaptureStart,		//enables a one shot capture process
	eAudioCaptureStop,		//stops the current capture process
	eAudioBGCaptureStart, 	//enable a bg capture process that always gets restarted
} EAudioCommand_t;

typedef struct {
	int32_t volume;
	int32_t durationInSeconds;
	uint32_t fade_in_ms;
	uint32_t fade_out_ms;
	uint32_t to_fade_out_ms;
	uint32_t rate;
	hlo_stream_t * stream;
	NotificationCallback_t onFinished;
	hlo_filter p;				/* the algorithm to run on the spkr input */
	void * context;
	char source_name[MAX_SOURCE_NAME_CHAR];
} AudioPlaybackDesc_t;

typedef struct {
	EAudioCommand_t command;

	union {
		AudioCaptureDesc_t capturedesc;
		AudioPlaybackDesc_t playbackdesc;
	} message;

} AudioMessage_t;

void AudioPlaybackTask(void * data);
void AudioCaptureTask(void * data);

#include "codec_debug_config.h"

void AudioTask_StartPlayback(const AudioPlaybackDesc_t * desc);

void AudioTask_StopPlayback(void);
/**
 * stops all capture processes
 */
void AudioTask_StopCapture(void);
/**
 * enables the default background capture process
 */
void AudioTask_StartCapture(uint32_t rate);
/**
 * queues a one shot capture process.
 */
void AudioTask_QueueCaptureProcess(const AudioCaptureDesc_t * desc);

void AudioTask_DumpOncePerMinuteStats(AudioOncePerMinuteData_t * pdata);
/**
 * testing commands
 */
int Cmd_AudioPlayback(int argc, char * argv[]);
int Cmd_AudioCapture(int argc, char * argv[]);

#endif //_AUDIOCAPTURETASK_H_
