#ifndef __CODEC_DEBUG_CONFIG_H__
#define __CODEC_DEBUG_CONFIG_H__

#define CODEC_ADC_32KHZ    		  1 // Set to 1 if DAC Fs= ADC Fs= 16k Hz. If not, ADC Fs = DAC Fs = 48k Hz

// Left mic data is latched on rising by default, to latch it on rising edge instead
// set this to be 1
#define CODEC_LEFT_LATCH_FALLING 	1

#endif
