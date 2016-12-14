#ifndef _AUDIOCAPTURETASK_H_
#define _AUDIOCAPTURETASK_H_

#include <stdint.h>
#include "audio_types.h"

#include "hlo_stream.h"
#include "hlo_pipe.h"

#include "codec_debug_config.h"

typedef enum {
	eAudioPlaybackStart,
	eAudioPlaybackStop,
	eAudioResetCodec,
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
	NotificationCallback_t onInterrupt;
	NotificationCallback_t onPlay; // in case of interruption this will be called before each resume
	hlo_filter p;				/* the algorithm to run on the spkr input */
	void * context;
	char source_name[64];
} AudioPlaybackDesc_t;



typedef struct {
	EAudioCommand_t command;

	union {
		AudioPlaybackDesc_t playbackdesc;
	} message;

} AudioMessage_t;

void AudioPlaybackTask(void * data);

#include "codec_debug_config.h"

void AudioTask_StartPlayback(const AudioPlaybackDesc_t * desc);
void AudioTask_ResetCodec(void);

void AudioTask_StopPlayback(void);
void AudioTask_DumpOncePerMinuteStats(AudioEnergyStats_t * pdata);
/**
 * testing commands
 */
int Cmd_AudioPlayback(int argc, char * argv[]);

#endif //_AUDIOCAPTURETASK_H_
