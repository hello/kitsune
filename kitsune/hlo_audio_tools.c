#include "hlo_audio_tools.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"
#include "octogram.h"
#include "audio_types.h"
#include "audiofeatures.h"
#include "hlo_async.h"
#include "hlo_audio.h"
#include <stdbool.h>

////-------------------------------------------
//recorder sample app
#define CHUNK_SIZE 1024
int audio_sig_stop = 0;
void hlo_audio_recorder_task(hlo_stream_t * out){
	int ret;
	uint8_t chunk[CHUNK_SIZE];
	hlo_stream_t * mic;
	mic = hlo_audio_open_mono(16000, 44, HLO_AUDIO_RECORD);

	while( (ret = hlo_stream_transfer_between(mic,out,chunk, sizeof(chunk),4)) > 0){
		if(audio_sig_stop){
			break;
		}
	}
	hlo_stream_close(mic);
	hlo_stream_close(out);
	DISP("Recorder Task Finished %d\r\n", ret);
}

////-------------------------------------------
//octogram sample app
#define PROCESSOR_BUFFER_SIZE ((AUDIO_FFT_SIZE)*3*2)
#define OCTOGRAM_DURATION 500
void hlo_audio_octogram_task(void * data){
	Octogram_t octogramdata = {0};
	int ret,i;
	int32_t duration = 500;
	Octogram_Init(&octogramdata);
	OctogramResult_t result;
	int16_t * samples = pvPortMalloc(PROCESSOR_BUFFER_SIZE);
	hlo_stream_t * input = (hlo_stream_t*)data;
	if(!samples){
		goto exit;
	}
	while( (ret = hlo_stream_transfer_all(FROM_STREAM,input,(uint8_t*)samples,PROCESSOR_BUFFER_SIZE,4)) > 0){
		//convert from 48K to 16K
		for(i = 0; i < 256; i++){
			int32_t sum = samples[i] + samples[AUDIO_FFT_SIZE+i] + samples[(2*AUDIO_FFT_SIZE)+i];
			samples[i] = (int16_t)(sum / 3);
		}
		Octogram_Update(&octogramdata,samples);
		DISP(".");
		if(duration-- < 0){
			DISP("\r\n");
			Octogram_GetResult(&octogramdata, &result);
			LOGF("octogram log energies: ");
			for (i = 0; i < OCTOGRAM_SIZE; i++) {
				if (i != 0) {
					LOGF(",");
				}
				LOGF("%d",result.logenergy[i]);
			}
			LOGF("\r\n");
			break;
		}
	}
	vPortFree(samples);
exit:
	DISP("Octogram Task Finished %d\r\n", ret);
	hlo_stream_close(input);
}
////-----------------------------------------
//commands
extern hlo_stream_t * open_stream_from_path(char * str, uint8_t input);
int Cmd_audio_record_start(int argc, char *argv[]){
	//audio_sig_stop = 0;
	//hlo_audio_recorder_task("rec.raw");
	AudioTask_StartCapture(16000);
	return 0;
}
int Cmd_audio_record_stop(int argc, char *argv[]){
	AudioTask_StopPlayback();
	audio_sig_stop = 1;
	return 0;

}
int Cmd_audio_record_replay(int argc, char *argv[]){
	DISP("Playing back %s ...\r\n", argv[1]);
	AudioPlaybackDesc_t desc;
	desc.durationInSeconds = 10;
	desc.fade_in_ms = 1000;
	desc.fade_out_ms = 1000;
	desc.stream = fs_stream_open_media(argv[1],0);
	desc.volume = 50;
	desc.onFinished = NULL;
	desc.rate = argc > 2?atoi(argv[2]):48000;
	audio_sig_stop = 0;
	AudioTask_StartPlayback(&desc);
	return 0;
}
int Cmd_audio_octogram(int argc, char *argv[]){
	audio_sig_stop = 0;
	//hlo_app_audio_octogram_task(random_stream_open());

	hlo_audio_octogram_task(open_stream_from_path(argv[1],2));
	return 0;
}
int Cmd_audio_features(int argc, char *argv[]){
	audio_sig_stop = 0;
	hlo_audio_feature_extraction_task(open_stream_from_path(argv[1],1));
	return 0;
}

