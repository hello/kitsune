#include "audiocontrolhelper.h"
#include "audiotask.h"
#include "audioprocessingtask.h"


void AudioControlHelper_SetAudioControl(AudioControl * pcontrol) {

	if (!pcontrol) {
		return;
	}

	if (pcontrol->has_audio_capture_action) {
		switch (pcontrol->audio_capture_action) {
		case AudioControl_AudioCaptureAction_ON:
		{
			AudioTask_StartCapture(16000);
			break;
		}

		case AudioControl_AudioCaptureAction_OFF:
		{
			AudioTask_StopCapture();
			break;
		}

		default:
		{
			break;
		}
		}
	}

	if (pcontrol->has_audio_save_features) {
		switch (pcontrol->audio_save_features) {
		case AudioControl_AudioCaptureAction_ON:
		{
			//turn on uploading of audio MFCC features
			AudioProcessingTask_SetControl(featureUploadsOn,NULL,NULL);
			break;
		}

		case AudioControl_AudioCaptureAction_OFF:
		{
			//turn off uploading of audio MFCC features
			AudioProcessingTask_SetControl(featureUploadsOff,NULL,NULL);
			break;
		}

		default:
		{
			break;
		}
		}
	}

	if (pcontrol->has_audio_save_raw_data) {
		switch (pcontrol->audio_save_raw_data) {
		case AudioControl_AudioCaptureAction_ON:
		{
			//turn on uploading of raw audio
			AudioProcessingTask_SetControl(rawUploadsOn,NULL,NULL);
			break;
		}

		case AudioControl_AudioCaptureAction_OFF:
		{
			//turn off uploading of raw audio
			AudioProcessingTask_SetControl(rawUploadsOff,NULL,NULL);
			break;
		}

		default:
		{
			break;
		}
		}
	}

}



void AudioControlHelper_SetPillSettings(const SyncResponse_PillSettings * settings, uint16_t count) {

	uint16_t i;

	for (i = 0; i < count; i++) {
		AudioTask_AddPillId(settings[i].pill_id,i==0);
	}


}


