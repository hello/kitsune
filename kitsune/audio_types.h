#ifndef _AUDIOTYPES_H_
#define _AUDIOTYPES_H_

#include <stdint.h>
#include "hlo_stream.h"
#include "hlo_pipe.h"

#include "codec_debug_config.h"

#define AUDIO_SAMPLE_RATE 32000

//magic number, in no particular units, just from observation
#define MIN_CLASSIFICATION_ENERGY (100)


#define SAMPLE_RATE_IN_HZ (66)
#define SAMPLE_PERIOD_IN_MILLISECONDS  (1000 / SAMPLE_RATE_IN_HZ)

/*
 // use simplelink.h instead
#define TRUE (1)
#define FALSE (0)
*/




#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    int64_t samplecount;
    int16_t logenergy;
} AudioFeatures_t;
    

#define AUDIO_TRANSFER_FLAG_AUTO_CLOSE_OUTPUT     (1 << 0) /* automatically close output stream when done */


typedef struct {
	uint32_t rate;
	hlo_stream_t * opt_out;		/* optional output */
	hlo_filter p;				/* the algorithm to run on the mic input */
	uint32_t flag;				/* flag option, see @AUDIO_TRANSFER_FLAG_ */
	void * ctx;					/* optional ctx pointer */
} AudioCaptureDesc_t;

/* ----------------- */

typedef struct {
	uint8_t mac[6];
	char device_id[32];
	uint32_t unix_time;
} DeviceCurrentInfo_t;


typedef struct {
	int32_t num_disturbances;
	int32_t num_samples;
	int32_t peak_energy;
	int32_t peak_background_energy;
	int32_t disturbance_time_count;
	uint8_t isValid;
} AudioEnergyStats_t;

typedef void (*AudioFeatureCallback_t)(const AudioFeatures_t * pfeats);
typedef void (*AudioEnergyStatsCallback_t) (const AudioEnergyStats_t * pdata);
typedef void (*NotificationCallback_t)(void * context);
typedef void (*RecordAudioCallback_t)(const AudioCaptureDesc_t * request);
    
#ifdef __cplusplus
}
#endif


#endif
