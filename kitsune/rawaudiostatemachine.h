#ifndef _RAWAUDIOSTATEMACHINE_H_
#define _RAWAUDIOSTATEMACHINE_H_

#include "audio_types.h"

void RawAudioStateMachine_Init(RecordAudioCallback_t callback);
void RawAudioStateMachine_SetLogLikelihoodOfModel(int32_t lik, int32_t threshold);
void RawAudioStateMachine_IncrementSamples(void);

#endif
