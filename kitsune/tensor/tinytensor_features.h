#ifndef _TINYTENSOR_FEATURES_H_
#define _TINYTENSOR_FEATURES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "tinytensor_types.h"
#define NUM_SAMPLES_TO_RUN_FFT (160)
#define FFT_UNPADDED_SIZE (400)
#define BUF_SIZE_IN_SAMPLES (600)
#define NUM_MEL_BINS (40)
#define MEL_FEAT_BUF_TIME_LEN (157)
    
    
typedef void(*tinytensor_audio_feat_callback_t)(void * context, int16_t * feats);
    
typedef enum {
    start_speech,
    stop_speech
} SpeechTransition_t;
    
typedef void(*tinytensor_speech_detector_callback_t)(void * context, SpeechTransition_t transition);
    
void tinytensor_features_initialize(void * results_context, tinytensor_audio_feat_callback_t results_callback,tinytensor_speech_detector_callback_t speech_detector_callback);

void tinytensor_features_deinitialize(void);

void tinytensor_features_add_samples(const int16_t * samples, const uint32_t num_samples);

void tinytensor_features_force_voice_activity_detection(void);
    
    
#ifdef __cplusplus
}
#endif

#define OLD_NET

#endif //_TINYTENSOR_FEATURES_H_
