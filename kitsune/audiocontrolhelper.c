#include "audiocontrolhelper.h"
#include "audiotask.h"
#include "audioprocessingtask.h"
#include "uart_logger.h"


void AudioControlHelper_SetAudioControl(AudioControl * pcontrol) {

	if (!pcontrol) {
		return;
	}

	if (pcontrol->has_audio_capture_action) {
		switch (pcontrol->audio_capture_action) {
		case AudioControl_AudioCaptureAction_ON:
		{
			// AudioTask_StartCapture(AUDIO_CAPTURE_PLAYBACK_RATE);
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
			AudioProcessingTask_SetControl(featureUploadsOn, 1000);
			break;
		}

		case AudioControl_AudioCaptureAction_OFF:
		{
			//turn off uploading of audio MFCC features
			AudioProcessingTask_SetControl(featureUploadsOff, 1000);
			break;
		}

		default:
		{
			break;
		}
		}
	}

	if (pcontrol->has_audio_save_raw_data) {
		LOGI("Raw Audio Upload Not Supported\r\n");
	}

}
