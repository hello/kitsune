#ifndef _FEATURESTYPES_H_
#define _FEATURESTYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define NUM_SAMPLES_TO_RUN_FFT (240)
#define FFT_UNPADDED_SIZE (400)
#define BUF_SIZE_IN_SAMPLES (600)
#define NUM_MEL_BINS (40)
#define MEL_FEAT_BUF_TIME_LEN (157)

#define FEATURES_FFT_SIZE_2N (9)
#define FEATURES_FFT_SIZE (1 << FEATURES_FFT_SIZE_2N)

typedef void(*tinytensor_audio_feat_callback_t)(void * context, int16_t * feats);

typedef enum {
    start_speech,
    stop_speech
} SpeechTransition_t;

typedef void(*tinytensor_speech_detector_callback_t)(void * context, SpeechTransition_t transition);

#ifdef __cplusplus
}
#endif

    
#endif //FEATURESTYPES_H_
