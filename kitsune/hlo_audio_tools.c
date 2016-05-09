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

	if(pdata->num_disturbances){
		LOGI("audio disturbance: %d,  background=%d, peak=%d, samples = %d\r",pdata->num_disturbances, pdata->peak_background_energy, _stats.peak_energy , _stats.num_samples);
		LOGI("\n");
	}
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
int hlo_filter_feature_extractor(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	uint8_t buf[RECORD_SAMPLE_SIZE] = {0};
	static int64_t _callCounter;	/** time context for feature extractor **/
	uint32_t settle_count = 0;		/** let audio settle after playback **/
	int ret = 0;
	if(!_statsMutex){
		_statsMutex = xSemaphoreCreateMutex();
		assert(_statsMutex);
		AudioFeatures_Init(DataCallback,StatsCallback);
	}
	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM,input,buf,sizeof(buf),4);
		if(ret < 0){
			break;
		}else if(settle_count++ > 3){
			AudioFeatures_SetAudioData((const int16_t*)buf,_callCounter++);
		}
		hlo_stream_transfer_all(INTO_STREAM,output,buf,ret,4); /** be a good samaritan and transfer the stream back out */
		BREAK_ON_SIG(signal);
	}
	LOGI("Feature Extractor Completed:");
	LOGI("%d disturbances, %d/%d dB(p/bg), %u samples\r\n", _stats.num_disturbances, _stats.peak_energy, _stats.peak_background_energy, _stats.num_samples);
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
//adpcm processor pair
//TODO make it more efficient
#include "adpcm.h"
#define ADPCM_SAMPLES (512)
int hlo_filter_adpcm_decoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	char compressed[ADPCM_SAMPLES/2];
	short decompressed[ADPCM_SAMPLES];
	adpcm_state state = (adpcm_state){0};
	int ret = 0;
	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM, input, compressed, ADPCM_SAMPLES/2,4);
		if( ret < 0 ){
			break;
		}
		adpcm_decoder((char*)compressed, (short*)decompressed, ret * 2 , &state);
		if( output ){
			ret = hlo_stream_transfer_all(INTO_STREAM, output, decompressed, ret * 4, 4);
			if ( ret < 0 ){
				break;
			}
		}
		BREAK_ON_SIG(signal);
	}
	return ret;
}
int hlo_filter_adpcm_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	char compressed[ADPCM_SAMPLES/2];
	short decompressed[ADPCM_SAMPLES];
	adpcm_state state = (adpcm_state){0};
	int ret = 0;
	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM, input, decompressed,ADPCM_SAMPLES * 2, 4);
		if ( ret < 0 ){
			break;
		}
		adpcm_coder((short*)decompressed, (char*)compressed, ret / 2, &state);
		if( output ){
			ret = hlo_stream_transfer_all(INTO_STREAM, output, compressed, ret / 4, 4);
			if (ret < 0 ){
				break;
			}
		}
		BREAK_ON_SIG(signal);
	}
	return ret;
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
static uint8_t _can_has_sig_stop(void){
	return audio_sig_stop;
}
int Cmd_audio_record_start(int argc, char *argv[]){
	//audio_sig_stop = 0;
	//hlo_audio_recorder_task("rec.raw");
	AudioTask_StartCapture(16000);
	return 0;
}
int Cmd_audio_record_stop(int argc, char *argv[]){
	DISP("Stopping Audio\r\n");
	AudioTask_StopPlayback();
	audio_sig_stop = 1;
	return 0;

}
int Cmd_audio_adpcm_encode(int argc, char *argv[]){
	if(argc < 3){
		return -1;
	}
	int ret;
	audio_sig_stop = 0;
	hlo_stream_t * in = open_stream_from_path(argv[1], 1);
	hlo_stream_t * out = open_stream_from_path(argv[2], 0);
	ret = hlo_filter_adpcm_encoder(in, out, NULL, _can_has_sig_stop);
	hlo_stream_close(in);
	hlo_stream_close(out);
	LOGI("Encoder Finished %d\r\n", ret);
	return 0;
}
int Cmd_audio_adpcm_decode(int argc, char *argv[]){
	if(argc < 3){
		return -1;
	}
	int ret;
	hlo_stream_t * in = open_stream_from_path(argv[1], 1);
	hlo_stream_t * out = open_stream_from_path(argv[2], 0);
	ret = hlo_filter_adpcm_decoder(in, out, NULL, _can_has_sig_stop);
	hlo_stream_close(in);
	hlo_stream_close(out);
	LOGI("Decoder Finished %d\r\n", ret);
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
	hlo_stream_t * s = open_stream_from_path(argv[1],1);
	hlo_filter_feature_extractor(s, NULL, NULL, _can_has_sig_stop);
	return 0;
}

