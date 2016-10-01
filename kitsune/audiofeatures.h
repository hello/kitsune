#ifndef _AUDIOFEATURES_H_
#define _AUDIOFEATURES_H_

#include <stdint.h>

#include "audio_types.h"



#ifdef __cplusplus
extern "C" {
#endif

    


/*  exported for your enjoyment -- use these! */
void init_background_energy(AudioOncePerMinuteDataCallback_t fpOncePerMinuteCallback);
    
/*  Expects FEATURES_FFT_SIZE samples in samplebuf  */
void set_background_energy(const int16_t fr[], const int16_t fi[], int16_t log2scale);

#ifdef __cplusplus
}
#endif
    
#endif //_AUDIOFEATURES_H_
