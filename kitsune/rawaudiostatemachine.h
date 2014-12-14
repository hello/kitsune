#ifndef _RAWAUDIOSTATEMACHINE_H_
#define _RAWAUDIOSTATEMACHINE_H_

#include "audio_types.h"

void RawAudioStateMachine_Init(RecordAudioCallback_t callback);
void RawAudioStateMachine_SetProbabilityOfDesiredClass(int8_t probQ7);

#endif
