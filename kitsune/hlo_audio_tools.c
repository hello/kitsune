#include "hlo_audio_tools.h"
#include "hlo_audio_manager.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"
#include "octogram.h"
#include "audio_types.h"
#include "audiofeatures.h"

////-------------------------------------------
//recorder sample app
#define CHUNK_SIZE 512
int audio_sig_stop = 0;
void hlo_audio_recorder_task(void * data){
	int ret;
	uint8_t chunk[CHUNK_SIZE];
	hlo_stream_t *fs, * mic;
	mic = hlo_open_mic_stream(0);
	fs = fs_stream_open_wlimit((char*)data, 48000 * 6); //max six seconds of audio

	while( (ret = hlo_stream_transfer_between(mic,fs,chunk, sizeof(chunk),4)) > 0){
		if(audio_sig_stop){
			break;
		}
	}

	hlo_stream_close(fs);
	hlo_stream_close(mic);
	DISP("Recorder Task Finished %d\r\n", ret);
}

////-------------------------------------------
//playback sample app
void hlo_audio_playback_task(void * data){
	int ret;
	uint8_t chunk[CHUNK_SIZE];
	hlo_stream_t * spkr = hlo_open_spkr_stream(2*CHUNK_SIZE);
	hlo_stream_t * fs = fs_stream_open_media((char*)data, 0);

	while( (ret = hlo_stream_transfer_between(fs,spkr,chunk, sizeof(chunk),4)) > 0){
		if(audio_sig_stop){
			audio_sig_stop = 0;
			break;
		}
	}

	hlo_stream_close(fs);
	hlo_stream_close(spkr);
	DISP("Playback Task Finished %d\r\n", ret);

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
	if(!samples){
		goto exit;
	}
	hlo_stream_t * input = (hlo_stream_t*)data;//hlo_open_mic_stream(CHUNK_SIZE*2, 1);
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
////-------------------------------------------
//feature xtraction sample app

static int64_t _callCounter;
static void DataCallback(const AudioFeatures_t * pfeats) {
	//AudioProcessingTask_AddFeaturesToQueue(pfeats);
	//DISP("audio feature: logEg: %d, overbg: %dr\n", pfeats->logenergy, pfeats->logenergyOverBackroundNoise);
}
static void StatsCallback(const AudioOncePerMinuteData_t * pdata) {
	LOGI("audio disturbance: background=%d, peak=%d\n",pdata->peak_background_energy,pdata->peak_energy);
}

void hlo_audio_feature_extraction_task(void * data){
	hlo_stream_t * input = (hlo_stream_t*)data;
	AudioFeatures_Init(DataCallback,StatsCallback);
	int16_t * samples = pvPortMalloc(PROCESSOR_BUFFER_SIZE);
	int i,ret;
	if(!samples){
		goto exit;
	}
	while( (ret = hlo_stream_transfer_all(FROM_STREAM,input,(uint8_t*)samples,PROCESSOR_BUFFER_SIZE,4)) > 0){
		for(i = 0; i < 256; i++){
			int32_t sum = samples[i] + samples[AUDIO_FFT_SIZE+i] + samples[(2*AUDIO_FFT_SIZE)+i];
			samples[i] = (int16_t)(sum / 3);
		}
		AudioFeatures_SetAudioData(samples,_callCounter++);
		if(audio_sig_stop){
			break;
		}
	}
	vPortFree(samples);
exit:
	DISP("Audio Feature Task Finished %d\r\n", ret);
	hlo_stream_close(input);

}
////-----------------------------------------
//commands
extern hlo_stream_t * open_stream_from_path(char * str, uint8_t input);
int Cmd_audio_record_start(int argc, char *argv[]){
	audio_sig_stop = 0;
	hlo_audio_recorder_task("rec.raw");
	return 0;
}
int Cmd_audio_record_stop(int argc, char *argv[]){
	audio_sig_stop = 1;
	return 0;

}
int Cmd_audio_record_replay(int argc, char *argv[]){
	DISP("Playing back %s ...\r\n", argv[1]);
	audio_sig_stop = 0;
	hlo_audio_playback_task(argv[1]);
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

