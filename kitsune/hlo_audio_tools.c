#include "hlo_audio_tools.h"
#include "task.h"
#include "kit_assert.h"
#include "hellofilesystem.h"
#include "hlo_pipe.h"
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

int audio_sig_stop = 0;

static void DataCallback(const AudioFeatures_t * pfeats) {
//	LOGI("Found Feature\r\n");
	AudioProcessingTask_AddFeaturesToQueue(pfeats);
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
	if(!_statsMutex){
		_statsMutex = xSemaphoreCreateMutex();
		assert(_statsMutex);
	}
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
#define ADPCM_SAMPLES (1024)
int hlo_filter_adpcm_decoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	char compressed[ADPCM_SAMPLES/2];
	short decompressed[ADPCM_SAMPLES];
	adpcm_state state = (adpcm_state){0};
	int ret = 0;
	hlo_stream_t * decoded =  fs_stream_open("/decoded", HLO_STREAM_WRITE);
	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)compressed, ADPCM_SAMPLES/2,4);
		if( ret < 0 ){
			break;
		}
		adpcm_decoder((char*)compressed, (short*)decompressed, ret * 2 , &state);
		if( output ){
			int transfer_size = ret * 4;
			ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)decompressed, ret * 4, 4);
			hlo_stream_transfer_all(INTO_STREAM, decoded, (uint8_t*)decompressed,transfer_size, 4);
			if ( ret < 0 ){
				break;
			}
		}
		BREAK_ON_SIG(signal);
	}
	hlo_stream_close(decoded);
	return ret;
}
int hlo_filter_adpcm_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	char compressed[ADPCM_SAMPLES/2];
	short decompressed[ADPCM_SAMPLES];
	adpcm_state state = (adpcm_state){0};
	int ret = 0;
	while(1){
		ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)decompressed,ADPCM_SAMPLES * 2, 4);
		if ( ret < 0 ){
			break;
		}
		adpcm_coder((short*)decompressed, (char*)compressed, ret / 2, &state);
		if( output ){
			ret = hlo_stream_transfer_all(INTO_STREAM, output, (uint8_t*)compressed, ret / 4, 4);
			if (ret < 0 ){
				break;
			}
		}
		BREAK_ON_SIG(signal);
	}
	return ret;
}
////-------------------------------------------
// Simple data transfer filter
int hlo_filter_data_transfer(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	uint8_t buf[512];
	int ret;
	while(1){
		ret = hlo_stream_transfer_between(input,output,buf,sizeof(buf),4);
		if(ret < 0){
			break;
		}
		BREAK_ON_SIG(signal);
	}
	return ret;
}
////-------------------------------------------
// Throughput calculator
int hlo_filter_throughput_test(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	TickType_t start = xTaskGetTickCount();
	int ret = hlo_filter_data_transfer(input, output, ctx, signal);
	TickType_t tdelta = xTaskGetTickCount() - start;
	int ts = tdelta / 1000;
	if(ts == 0){
		ts = 1;
	}
	size_t total =  output->info.bytes_written;
	LOGI("Transferred %u bytes over %u milliseconds, throughput %u kb/s\r\n", total, tdelta, (total / 1024) / ts);
	return ret;
}
////-------------------------------------------
//octogram sample app
#include "octogram.h"
#define PROCESSOR_BUFFER_SIZE ((AUDIO_FFT_SIZE)*3*2)
#define OCTOGRAM_DURATION 500
int hlo_filter_octogram(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	Octogram_t octogramdata = {0};
	int ret,i;
	int32_t duration = 500;
	Octogram_Init(&octogramdata);
	OctogramResult_t result;
	int16_t * samples = pvPortMalloc(PROCESSOR_BUFFER_SIZE);
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
		BREAK_ON_SIG(signal);
	}
	vPortFree(samples);
	DISP("Octogram Task Finished %d\r\n", ret);
	return ret;
}
////-------------------------------------------
//octogram sample app
extern uint8_t get_alpha_from_light();
#include "led_animations.h"
#include "nanopb/pb_decode.h"
#include "protobuf/response.pb.h"
#include "hlo_http.h"
extern bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg);
int hlo_filter_voice_command(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
#define NSAMPLES 512
	int sample_rate = AUDIO_CAPTURE_PLAYBACK_RATE;
	int ret = 0;
	int16_t samples[NSAMPLES];
	int32_t count = 0;
	int32_t zcr = 0;
	int32_t window_zcr;
	int32_t window_eng;
	int64_t eng = 0;
	uint8_t window_over = 0;
	bool light_open = false;

	output = hlo_stream_en( output );
	while( (ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)samples, sizeof(samples), 4)) > 0 ){

		if( !light_open ) {
			input = hlo_light_stream( input );
			light_open = true;
		}

		ret = hlo_stream_transfer_all(INTO_STREAM, output,  (uint8_t*)samples, ret, 4);
		if ( ret <  0){
			break;
		}
		BREAK_ON_SIG(signal);
	}

	{//now play the swirling thing when we get response
			play_led_wheel(get_alpha_from_light(),254,0,254,2,18,0);
			DISP("Wheel\r\n");
	}
#if 0
	//lastly, glow with voice output, since we can't do that in half duplex mode, simply queue it to the voice output
	if( ret >= 0){
		SpeechResponse resp = SpeechResponse_init_zero;
		DISP("\r\n===========\r\n");
		resp.text.funcs.decode = _decode_string_field;
		resp.url.funcs.decode = _decode_string_field;
		if( 0 == hlo_pb_decode(output,SpeechResponse_fields, &resp) ){
			DISP("Resp %s\r\nUrl %s\r\n", resp.text.arg, resp.url.arg);
			if(resp.audio_stream_size){
				hlo_stream_t * aud = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE, 60,HLO_AUDIO_PLAYBACK);
				DISP("Playback Audio\r\n");
				hlo_filter_adpcm_decoder(output,aud,NULL,NULL);
			}
		/*	hlo_stream_t * aud = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE, 60,HLO_AUDIO_PLAYBACK);
			hlo_stream_t * fs = hlo_http_get(resp.url.arg);
			hlo_filter_adpcm_decoder(fs,aud,NULL,NULL);
			hlo_stream_close(fs);
			hlo_stream_close(aud);
		*/
			vPortFree(resp.text.arg);
			vPortFree(resp.url.arg);
		}else{
			DISP("Decoded Failed\r\n");
		}
		DISP("\r\n===========\r\n");
	}
#else
	if(ret >= 0 || ret == HLO_STREAM_EOF ){
		DISP("\r\n===========\r\n");
		hlo_stream_t * aud = hlo_audio_open_mono(AUDIO_CAPTURE_PLAYBACK_RATE, 60,HLO_AUDIO_PLAYBACK);
						DISP("Playback Audio\r\n");
						hlo_filter_adpcm_decoder(output,aud,NULL,NULL);
						DISP("\r\n===========\r\n");
		hlo_stream_close(aud);
	}
#endif
	return ret;
}
#include "hellomath.h"
int hlo_filter_modulate_led_with_sound(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	int ret;
	int16_t samples[NSAMPLES] = {0};
	play_modulation(253,253,253,30,0);
	int32_t reduced = 0;
	int32_t lp = 0;
	int32_t last_eng = 0;
	while( (ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)samples, sizeof(samples), 4)) >= 0 ){
		int i;
		int32_t eng = 0;
		for(i = 0; i < NSAMPLES; i++){
			eng += abs(samples[i]);
		}
		eng = fxd_sqrt(eng/NSAMPLES);

		reduced = 3 * reduced >> 2;
		reduced += abs(eng - last_eng)<<1;

		lp += ( reduced - lp ) >> 3;
		//DISP("%d %d %d\n", lp, reduced, abs((fxd_sqrt(eng) - last_eng))) ;

		last_eng = eng;

		if(lp > 253){
			lp = 253;
		}
		if( lp < 20 ){
			lp = 20;
		}
		set_modulation_intensity( lp );
		hlo_stream_transfer_all(INTO_STREAM, output,  (uint8_t*)samples, ret, 4);
		BREAK_ON_SIG(signal);
	}
	stop_led_animation( 0, 33 );
	return ret;
}
#include "tensor/keyword_net.h"
typedef struct{
	hlo_stream_t * base;
	uint8_t keyword_detected;
	uint8_t threshold;
	uint16_t reserved;
	uint32_t timeout;
}nn_keyword_ctx_t;

static void _begin_keyword(void * ctx, Keyword_t keyword, int8_t value){
	play_led_animation_solid(254, 254, 254, 254 ,1, 18,3);
	DISP("Keyword Start\r\n");
}
static void _finish_keyword(void * ctx, Keyword_t keyword, int8_t value){
	DISP("Keyword Done\r\n");
	if(ctx){
		((nn_keyword_ctx_t *)ctx)->keyword_detected = 1;
	}
}
//note that filter and the stream version can not run concurrently
int hlo_filter_nn_keyword_recognition(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	int16_t samples[160];
	int ret;
	keyword_net_initialize();

	keyword_net_register_callback(0,okay_sense,80,_begin_keyword,_finish_keyword);
	while( (ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)samples, sizeof(samples), 4)) >= 0 ){
		keyword_net_add_audio_samples(samples,ret/sizeof(int16_t));

		hlo_stream_transfer_all(INTO_STREAM, output,  (uint8_t*)samples, ret, 4);
		BREAK_ON_SIG(signal);
	}
	DISP("Keyword Detection Exit\r\n");
	keyword_net_deinitialize();
	return 0;
}
static int _close_nn_stream(void * ctx){
	nn_keyword_ctx_t * nn = (nn_keyword_ctx_t*)ctx;
	hlo_stream_t * base = nn->base;
	keyword_net_deinitialize();
	vPortFree(ctx);
	return hlo_stream_close(base);
}
static int _read_nn_stream(void * ctx, void * buf, size_t size){
	nn_keyword_ctx_t * nn = (nn_keyword_ctx_t*)ctx;
	if(nn->keyword_detected){
		return hlo_stream_read(nn->base, buf, size);
	}else{
		int16_t samples[160];
		int ret = hlo_stream_transfer_all(FROM_STREAM, nn->base, (uint8_t*)samples, sizeof(samples), 4);
		if(ret % 2){//alignment error
			return HLO_STREAM_ERROR;
		}else if(ret > 0){
			keyword_net_add_audio_samples(samples, ret/sizeof(int16_t));
		}else if(ret < 0){
			return ret;
		}
		return 0;
	}
}

hlo_stream_t * hlo_stream_nn_keyword_recognition(hlo_stream_t * base, uint8_t threshold){
	hlo_stream_vftbl_t tbl = (hlo_stream_vftbl_t){
		.write = NULL,
		.read = _read_nn_stream,
		.close = _close_nn_stream,
	};
	nn_keyword_ctx_t * ctx = pvPortMalloc(sizeof(*ctx));
	hlo_stream_t * ret = NULL;
	if(!ctx) return NULL;
	memset(ctx, 0, sizeof(*ctx));
	ctx->threshold = threshold;
	ctx->base = base;
	keyword_net_initialize();
	keyword_net_register_callback(ctx, okay_sense, threshold, _begin_keyword, _finish_keyword);
	ret = hlo_stream_new(&tbl, ctx, HLO_STREAM_READ);
	if(!ret){
		vPortFree(ctx);
		keyword_net_deinitialize();
	}
	return ret;

}
////-----------------------------------------
//commands
static uint8_t _can_has_sig_stop(void){
	return audio_sig_stop;
}
int Cmd_audio_record_start(int argc, char *argv[]){
	//audio_sig_stop = 0;
	//hlo_audio_recorder_task("rec.raw");
	AudioTask_StartCapture(AUDIO_CAPTURE_PLAYBACK_RATE);
	return 0;
}
int Cmd_audio_record_stop(int argc, char *argv[]){
	DISP("Stopping Audio\r\n");
	AudioTask_StopPlayback();
	AudioTask_StopCapture();
	audio_sig_stop = 1;
	return 0;

}
static hlo_filter _filter_from_string(const char * str){
	switch(str[0]){
	case 'f':
		return hlo_filter_feature_extractor;
	case 'e':
		return hlo_filter_adpcm_encoder;
	case 'd':
		return hlo_filter_adpcm_decoder;
	case 'o':
		return hlo_filter_octogram;
	case '?':
		return hlo_filter_throughput_test;
	case 'x':
		return hlo_filter_voice_command;
	case 'X':
		return hlo_filter_modulate_led_with_sound;
	case 'n':
		return hlo_filter_nn_keyword_recognition;
	default:
		return hlo_filter_data_transfer;
	}

}
#include <stdlib.h>
hlo_stream_t * open_stream_from_path(char * str, uint8_t input);
int Cmd_stream_transfer(int argc, char * argv[]){
	audio_sig_stop = 0;
	hlo_filter f = hlo_filter_data_transfer;
	int ret;
	if(argc < 3){
		LOGI("Usage: x in out [rate] [filter]\r\n");
		LOGI("Press s to stop the transfer\r\n");
	}
	if(argc >= 4){
		f = _filter_from_string(argv[3]);
	}
#if 0
	hlo_stream_t * in = open_stream_from_path(argv[1],1);
#else
	hlo_stream_t * in = open_stream_from_path(argv[1],2); // TODO DKH
#endif
	hlo_stream_t * out = open_stream_from_path(argv[2],0);

	if(in && out){
		ret = f(in,out,NULL, _can_has_sig_stop);
	}


	LOGI("Stream transfer exited with code %d\r\n", ret);
	hlo_stream_close(in);
	hlo_stream_close(out);
	return 0;
}
