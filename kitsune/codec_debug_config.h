#ifndef __CODEC_DEBUG_CONFIG_H__
#define __CODEC_DEBUG_CONFIG_H__

#define CODEC_ENABLE_MULTI_CHANNEL 1 // 0 -> Stereo, 1 -> 4 channels
// Left mic data is latched on rising by default, to latch it on rising edge instead
// set this to be 1
#define CODEC_LEFT_LATCH_FALLING 	1

#define AUDIO_FULL_DUPLEX 1

#define AUDIO_RECORD_DECIMATE 0

#define AUDIO_CAPTURE_PLAYBACK_RATE 32000

#endif
