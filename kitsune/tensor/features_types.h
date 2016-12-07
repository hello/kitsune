#ifndef _FEATURESTYPES_H_
#define _FEATURESTYPES_H_

#include <stdint.h>

#define NUM_SAMPLES_TO_RUN_FFT (240)
#define FFT_UNPADDED_SIZE (400)
#define BUF_SIZE_IN_SAMPLES (600)
#define NUM_MEL_BINS (40)
#define MEL_FEAT_BUF_TIME_LEN (157)

#define FEATURES_FFT_SIZE_2N (9)
#define FEATURES_FFT_SIZE (1 << FEATURES_FFT_SIZE_2N)

#define TINYFEATS_FLAGS_NONE                            (0x00000000)
#define TINYFEATS_FLAGS_TRIGGER_PRIMARY_KEYWORD_INVALID (0x00000001)

typedef void(*tinytensor_audio_feat_callback_t)(void * context, int16_t * feats,const uint32_t flags);

typedef enum {
    start_speech,
    stop_speech
} SpeechTransition_t;

typedef void(*tinytensor_speech_detector_callback_t)(void * context, SpeechTransition_t transition);

#endif //FEATURESTYPES_H_
