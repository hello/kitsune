#include "features_types.h"
#include "audio_buffering.h"
#include <string.h>

typedef struct {
    int16_t storage_buf[FFT_UNPADDED_SIZE];
    uint32_t current_idx;
    uint32_t num_samples;
} AudioCircularBuf_t;

static AudioCircularBuf_t _data;

/*
    The idea is that audio samples will come in chunks, and we are trying 
    to fill a buffer of size FFT_UNPADDED_SIZE in the order the samples came in,
    but only do this when the number of new samples exceeds NUM_SAMPLES_TO_RUN_FFT.
 
 */

void audio_buffering_init(void) {
    memset(&_data,0,sizeof(_data));
}

#ifdef DESKTOP
int audio_buffering_add(int16_t * outbuf, const int16_t * samples, const uint32_t num_samples) {
#else
    __attribute__((section(".ramcode"))) int audio_buffering_add(int16_t * outbuf, const int16_t * samples, const uint32_t num_samples) {
#endif
    
    uint32_t num_samples_to_write = FFT_UNPADDED_SIZE - _data.current_idx;
    uint32_t num_samples_leftover = 0;
    
    if (num_samples_to_write >= num_samples) {
        num_samples_to_write = num_samples;
    }
    else {
        num_samples_leftover = num_samples - num_samples_to_write;
    }
    
    memcpy(&_data.storage_buf[_data.current_idx],&samples[0],num_samples_to_write*sizeof(int16_t));
    
    if (num_samples_leftover > 0) {
        memcpy(&_data.storage_buf[0], &samples[num_samples_to_write], num_samples_leftover*sizeof(int16_t));
    }
    
    _data.current_idx = (_data.current_idx + num_samples) % FFT_UNPADDED_SIZE;
    
    _data.num_samples += num_samples;
    
    if (_data.num_samples >= FFT_UNPADDED_SIZE) {
        num_samples_to_write = FFT_UNPADDED_SIZE - _data.current_idx;
        num_samples_leftover = FFT_UNPADDED_SIZE - num_samples_to_write;

        //fill buffer in order
        memcpy(&outbuf[0],&_data.storage_buf[_data.current_idx],num_samples_to_write*sizeof(int16_t));
        
        if (num_samples_leftover > 0) {
            memcpy(&outbuf[num_samples_to_write],&_data.storage_buf[0],num_samples_leftover*sizeof(int16_t));
        }
        
        _data.num_samples -= num_samples;
        
        return 1;
    }
    
    return 0;
    
}
