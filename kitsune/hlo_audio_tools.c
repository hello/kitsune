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
#include <string.h>
////-------------------------------------------
//The feature/extractor processor we used in sense 1.0
//
#include "audioprocessingtask.h"
#include "audiofeatures.h"
static xSemaphoreHandle _statsMutex = NULL;
static AudioOncePerMinuteData_t _stats;

static void DataCallback(const AudioFeatures_t * pfeats) {
//	LOGI("Found Feature\r\n");
	//AudioProcessingTask_AddFeaturesToQueue(pfeats);
}

static void StatsCallback(const AudioOncePerMinuteData_t * pdata) {

	xSemaphoreTake(_statsMutex,portMAX_DELAY);

	//LOGI("audio disturbance: %d,  background=%d, peak=%d, samples = %d\n",pdata->num_disturbances, pdata->peak_background_energy, _stats.peak_energy , _stats.num_samples);
	_stats.num_disturbances += pdata->num_disturbances;
	_stats.num_samples++;

        if (pdata->peak_background_energy > _stats.peak_background_energy) {
            _stats.peak_background_energy = pdata->peak_background_energy;
        }

	if (pdata->peak_energy > _stats.peak_energy) {
	    _stats.peak_energy = pdata->peak_energy;
	}
	_stats.isValid = 1;
	xSemaphoreGive(_statsMutex);
}
#define RECORD_SAMPLE_SIZE (256*2*2)
int hlo_filter_feature_extractor_processor(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	uint8_t buf[RECORD_SAMPLE_SIZE] = {0};
	static int64_t _callCounter;	/** time context for feature extractor **/
	uint32_t settle_count = 0;		/** let audio settle after playback **/
	int ret;
	if(!_statsMutex){
		_statsMutex = xSemaphoreCreateMutex();
		assert(_statsMutex);
		AudioFeatures_Init(DataCallback,StatsCallback);
	}
	while(1){
		int ret = hlo_stream_transfer_all(FROM_STREAM,input,buf,sizeof(buf),4);
		if(ret < 0){
			break;
		}else if(settle_count++ > 3){
			AudioFeatures_SetAudioData((const int16_t*)buf,_callCounter++);
		}
	}
	return ret;
}
void AudioTask_DumpOncePerMinuteStats(AudioOncePerMinuteData_t * pdata) {
	xSemaphoreTake(_statsMutex,portMAX_DELAY);
	memcpy(pdata,&_stats,sizeof(AudioOncePerMinuteData_t));
	pdata->peak_background_energy/=pdata->num_samples;
	memset(&_stats,0,sizeof(_stats));
	xSemaphoreGive(_statsMutex);
}
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
	//hlo_audio_feature_extraction_task(open_stream_from_path(argv[1],1));
	return 0;
}

