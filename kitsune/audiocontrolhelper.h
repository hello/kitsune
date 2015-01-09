#ifndef _AUDIOCONTROLHELPER_H_
#define _AUDIOCONTROLHELPER_H_

#include "protobuf/audio_control.pb.h"
#include "protobuf/sync_response.pb.h"

void AudioControlHelper_SetAudioControl(AudioControl * pcontrol);
void AudioControlHelper_SetPillSettings(const SyncResponse_PillSettings * settings, uint16_t count);
#endif 
