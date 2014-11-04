#ifndef _AUDIOPLAYBACKTASK_H_
#define _AUDIOPLAYBACKTASK_H_

#include <stdint.h>

typedef struct {
	const char * file;
	int32_t volume;

} AudioPlaybackDesc_t;

void AudioPlaybackTask_PlayFile(const AudioPlaybackDesc_t * playbackinfo);
void AudioPlaybackTask_Snooze(void);
void AudioPlaybackTask_StopPlayback(void);

void AudioPlaybackTask_Thread(void * data);


#endif //_AUDIOPLAYBACKTASK_H_
