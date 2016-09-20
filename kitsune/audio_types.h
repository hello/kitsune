#ifndef _AUDIOTYPES_H_
#define _AUDIOTYPES_H_

#include <stdint.h>
#include "hlo_stream.h"
#include "hlo_pipe.h"

#include "codec_debug_config.h"

#if (CODEC_ADC_32KHZ == 1)
#define AUDIO_SAMPLE_RATE 32000
#else
#define AUDIO_SAMPLE_RATE 48000
#endif

//magic number, in no particular units, just from observation
#define MIN_CLASSIFICATION_ENERGY (100)


#define AUDIO_FFT_SIZE_2N (8)
#define AUDIO_FFT_SIZE (1 << AUDIO_FFT_SIZE_2N)
#define EXPECTED_AUDIO_SAMPLE_RATE_HZ (AUDIO_SAMPLE_RATE)

#define SAMPLE_RATE_IN_HZ (EXPECTED_AUDIO_SAMPLE_RATE_HZ / AUDIO_FFT_SIZE)
#define SAMPLE_PERIOD_IN_MILLISECONDS  (1000 / SAMPLE_RATE_IN_HZ)

#define NUM_AUDIO_FEATURES (16)

#define OCTOGRAM_SIZE (AUDIO_FFT_SIZE_2N - 1)

/*
 // use simplelink.h instead
#define TRUE (1)
#define FALSE (0)
*/




#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t psd_min_energy;
    
} AudioFeaturesOptions_t;
    
typedef struct {
    uint16_t min_energy_classification;
} AudioClassifierOptions_t;

typedef enum {
    segmentCoherent
} ESegmentType_t;

typedef struct {
    int64_t t1;
    int64_t t2;
    int32_t duration;
    ESegmentType_t type;
    
} Segment_t;

typedef struct {
	int64_t samplecount;
    int16_t logenergy;
    int8_t feats4bit[NUM_AUDIO_FEATURES];
} AudioFeatures_t;
    

/* FOR SAVING AUDIO / UPLOADING AUDIO
 *
 * Yes, some of these flags are mutually exclusive.  Others aren ot.
 */

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
	int32_t logenergy[OCTOGRAM_SIZE];
} OctogramResult_t;

typedef struct {
	int32_t num_disturbances;
	int32_t num_samples;
	int32_t peak_energy;
	int32_t peak_background_energy;
	uint8_t isValid;
} AudioOncePerMinuteData_t;

typedef void (*SegmentAndFeatureCallback_t)(const int16_t * feats, const Segment_t * pSegment);
typedef void (*AudioFeatureCallback_t)(const AudioFeatures_t * pfeats);
typedef void (*AudioOncePerMinuteDataCallback_t) (const AudioOncePerMinuteData_t * pdata);
typedef void (*NotificationCallback_t)(void * context);
typedef void (*RecordAudioCallback_t)(const AudioCaptureDesc_t * request);
    
#ifdef __cplusplus
}
#endif


#endif
