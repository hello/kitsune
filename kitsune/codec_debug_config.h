#ifndef __CODEC_DEBUG_CONFIG_H__
#define __CODEC_DEBUG_CONFIG_H__

#define CODEC_ENABLE_MULTI_CHANNEL 1 // 0 -> Stereo, 1 -> 4 channels
#define CODEC_ADC_16KHZ    		0 // Set to 1 if DAC Fs= ADC Fs= 16k Hz. If not, ADC Fs = DAC Fs = 48k Hz
// Left mic data is latched on rising by default, to latch it on rising edge instead
// set this to be 1
#define CODEC_LEFT_LATCH_FALLING 	1

#define AUDIO_ENABLE_SIMULTANEOUS_TX_RX 0

#endif
