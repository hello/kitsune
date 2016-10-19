#ifndef _TINYTENSOR_FEATURES_H_
#define _TINYTENSOR_FEATURES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "tinytensor_types.h"
#include "features_types.h"
    
    
void tinytensor_features_initialize(void * results_context, tinytensor_audio_feat_callback_t results_callback,tinytensor_speech_detector_callback_t speech_detector_callback);

void tinytensor_features_deinitialize(void);

void tinytensor_features_add_samples(const int16_t * samples, const uint32_t num_samples);

void tinytensor_features_force_voice_activity_detection(void);
    
    
#ifdef __cplusplus
}
#endif


#endif //_TINYTENSOR_FEATURES_H_
