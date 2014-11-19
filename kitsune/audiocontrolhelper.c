#include "audiocontrolhelper.h"
#include "audiotask.h"


void AudioControlHelper_SetAudioControl(AudioControl * pcontrol) {

	if (pcontrol->has_audio_capture_action) {
		switch (pcontrol->audio_capture_action) {
		case AudioControl_AudioCaptureAction_ON:
		{
			AudioTask_StartCapture();
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

}
