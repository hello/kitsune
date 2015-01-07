#include "rawaudiostatemachine.h"
#include "hellomath.h"
#include <string.h>

#define NUM_SECONDS_TO_RECORD (30)
#define NUMBER_OF_SNORES_TO_HEAR_BEFORE_ACCEPTING (3)

static const int32_t  k_num_samples_to_record =  (SAMPLE_RATE_IN_HZ/2 * NUM_SECONDS_TO_RECORD); //account for the feature decimation
static const int32_t k_avg_prob_threshold_to_upload = TOFIX(0.5f,7);
typedef enum {
	notrecording,
	recording

} EState_t;

typedef struct {
	RecordAudioCallback_t callback;
	EState_t state;
	int32_t num_times_lik_exceeded_threshold;
	int32_t num_samples_recorded;
    int32_t num_samples_elapsed;
} RawAudioStateMachine_t;

static RawAudioStateMachine_t _data;


/* two states, recording and not recording.
 *
 * not recording:
 *   -If probability exceeds threshold, start recording
 *   -reset probability accumulation
 *   -set sample counter to zero
 *
 * recording
 *   -increase sample counter
 *   -accumulate probability
 *   -check if sample counter exceeds threshold, if so, compute average probability
 *   -if avg probability > threshold, set upload flag
 *   -terminate recording
 *
 *
 *
 */
void RawAudioStateMachine_Init(RecordAudioCallback_t callback) {
	memset(&_data,0,sizeof(_data));

	_data.callback = callback;
}

/*  This gets called every time through (32.5 hz) */
void RawAudioStateMachine_IncrementSamples(void) {
    _data.num_samples_elapsed++;
}


/*  This gets called once every second or so */
void RawAudioStateMachine_SetLogLikelihoodOfModel(int32_t lik, int32_t threshold) {

	AudioCaptureDesc_t desc;

	if (!_data.callback) {
		return;
	}


	switch (_data.state) {
	case notrecording:
	{
	    if (lik > threshold) {
	    	memset(&desc,0,sizeof(desc));

	    	_data.state = recording;
	    	_data.num_samples_recorded = 0;
	    	_data.num_times_lik_exceeded_threshold = 1;

	    	desc.change = startSaving;

			_data.callback(&desc);

	    }

		break;
	}

	case recording:
	{
        if (lik > threshold) {
            _data.num_times_lik_exceeded_threshold++;
        }
        
        _data.num_samples_recorded += _data.num_samples_elapsed;

		if (_data.num_samples_recorded > k_num_samples_to_record) {
			memset(&desc,0,sizeof(desc));

			_data.state = notrecording;


			desc.change = stopSaving;

            if (_data.num_times_lik_exceeded_threshold >= NUMBER_OF_SNORES_TO_HEAR_BEFORE_ACCEPTING) {
                //upload
                desc.flags |= AUDIO_TRANSFER_FLAG_UPLOAD;
				desc.flags |= AUDIO_TRANSFER_FLAG_DELETE_AFTER_UPLOAD;
			}
			else {
                //delete
                desc.flags |= AUDIO_TRANSFER_FLAG_DELETE_IMMEDIATELY;
			}

			_data.callback(&desc);

		}

		break;
	}

	default:
	{
		//should never get here!!!
		break;
	}

	}

    _data.num_samples_elapsed = 0;


}
