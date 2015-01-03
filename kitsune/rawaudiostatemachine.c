#include "rawaudiostatemachine.h"
#include "hellomath.h"
#include <string.h>

#define NUM_SECONDS_TO_RECORD (30)


static const int32_t  k_num_samples_to_record =  (SAMPLE_RATE_IN_HZ/2 * NUM_SECONDS_TO_RECORD); //account for the feature decimation
static const int32_t k_avg_prob_threshold_to_upload = TOFIX(0.5f,7);
typedef enum {
	notrecording,
	recording

} EState_t;

typedef struct {
	RecordAudioCallback_t callback;
	EState_t state;
	int32_t probaccumulator;
	int32_t num_samples_recorded;
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

void RawAudioStateMachine_SetProbabilityOfDesiredClass(int8_t probQ7) {

	AudioCaptureDesc_t desc;

	if (!_data.callback) {
		return;
	}


	switch (_data.state) {
	case notrecording:
	{
	    if (probQ7 > TOFIX(0.95f,7)) {
	    	memset(&desc,0,sizeof(desc));
	    //	UARTprintf("SWITCHING TO RECORD MODE--%d samples\r\n",k_num_samples_to_record);

	    	_data.state = recording;
	    	_data.num_samples_recorded = 0;
	    	_data.probaccumulator = 0;

	    	desc.change = startSaving;

			_data.callback(&desc);

	    }

		break;
	}

	case recording:
	{
		_data.probaccumulator += probQ7;
		_data.num_samples_recorded++;

		if (_data.num_samples_recorded > k_num_samples_to_record) {
			int32_t avgprob;
			memset(&desc,0,sizeof(desc));

			_data.state = notrecording;

			avgprob = _data.probaccumulator / _data.num_samples_recorded;

			desc.change = stopSaving;

		//	UARTprintf("STOPPING RECORDING, avgprob=%d\r\n",avgprob);

			if (avgprob > k_avg_prob_threshold_to_upload) {
			//	UARTprintf("let us upload the raw audio\r\n");
				desc.flags |= AUDIO_TRANSFER_FLAG_UPLOAD;
				desc.flags |= AUDIO_TRANSFER_FLAG_DELETE_AFTER_UPLOAD;
			}
			else {
				//UARTprintf("not uploading the raw audio\r\n");
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




}
