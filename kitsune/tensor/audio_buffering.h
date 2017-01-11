#ifndef _AUDIOBUFFERING_H_
#define _AUDIOBUFFERING_H_

#ifdef __cplusplus
extern "C" {
#endif

int audio_buffering_add(int16_t * outbuf, const int16_t * samples, const uint32_t num_samples);
    
void audio_buffering_init(void);
        
        
#ifdef __cplusplus
}
#endif


#endif //_AUDIOBUFFERING_H_
