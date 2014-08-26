#ifndef _AUDIOFEATURES_H_
#define _AUDIOFEATURES_H_

#include <stdint.h>

uint8_t AudioFeatures_MelAveraging(uint32_t idx, uint8_t x,uint8_t * buf, uint16_t * accumulator);

void AudioFeatures_Extract(uint8_t * melavg, const int16_t buf[],int16_t nfftsize);


#endif //_AUDIOFEATURES_H_