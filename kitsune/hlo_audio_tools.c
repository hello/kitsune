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
#include "ustdlib.h"
#include "hlo_proto_tools.h"
#include "crypto.h"
#include "wifi_cmd.h"
#include "i2c_cmd.h"
#include "sys_time.h"

#include "ble_proto.h"

#include "endpoints.h"

#include "kitsune_version.h"

#include "wifi_cmd.h"
#include "protobuf/state.pb.h"
bool audio_playing();

#include "speech.pb.h"
////-------------------------------------------
//The feature/extractor processor we used in sense 1.0
//
#include "audiofeatures.h"
#include "tensor/tinytensor_math_defs.h"

#define OKAY_SENSE_THRESHOLD     TOFIX(0.7)
#define OKAY_SENSE_MIN_DURATION  5

#define SNOOZE_THRESHOLD      TOFIX(0.2)
#define SNOOZE_MIN_DURATION   3

#define STOP_THRESHOLD        TOFIX(0.5)
#define STOP_MIN_DURATION     3

static xSemaphoreHandle _statsMutex = NULL;
static AudioEnergyStats_t _stats;

int audio_sig_stop = 0;

volatile bool disable_voice = false;

static void StatsCallback(const AudioEnergyStats_t * pdata) {

	xSemaphoreTake(_statsMutex,portMAX_DELAY);

	if(pdata->num_disturbances){
		LOGI("audio disturbance:ms=%d,bg=%d,peak=%d,peak_for_minute=%d,samples=%d\r\n",_stats.disturbance_time_count, pdata->peak_background_energy,pdata->peak_energy, _stats.peak_energy , _stats.num_samples);
		LOGI("\n");
	}
	_stats.num_disturbances += pdata->num_disturbances;
	_stats.disturbance_time_count += pdata->disturbance_time_count;
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
void AudioTask_DumpOncePerMinuteStats(AudioEnergyStats_t * pdata) {
	if(!_statsMutex){
		_statsMutex = xSemaphoreCreateMutex();
		assert(_statsMutex);
	}
	xSemaphoreTake(_statsMutex,portMAX_DELAY);

	//copy and reset
	memcpy(pdata,&_stats,sizeof(AudioEnergyStats_t));
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
#include "codec_runtime_update.h"
int hlo_filter_data_transfer(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	uint8_t buf[512];
	int ret;
	while(1){
		ret = hlo_stream_transfer_between(input,output,buf,sizeof(buf),4);

#if 0
		codec_test_runtime_prop_update();
#endif

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
#define PROCESSOR_BUFFER_SIZE ((OCTOGRAM_FFT_SIZE)*3*2)
#define OCTOGRAM_DURATION 10
int hlo_filter_octogram(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	Octogram_t octogramdata = {0};
	int ret,i;
	int32_t duration = OCTOGRAM_DURATION;
	Octogram_Init(&octogramdata);
	OctogramResult_t result;
	int16_t * samples = pvPortMalloc(PROCESSOR_BUFFER_SIZE);
	while( (ret = hlo_stream_transfer_all(FROM_STREAM,input,(uint8_t*)samples,PROCESSOR_BUFFER_SIZE,4)) > 0){
		//convert from 48K to 16K
		for(i = 0; i < 256; i++){
			int32_t sum = samples[i] + samples[OCTOGRAM_FFT_SIZE+i] + samples[(2*OCTOGRAM_FFT_SIZE)+i];
			samples[i] = (int16_t)(sum / 3);
		}
		Octogram_Update(&octogramdata,samples);
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

#include "tensor/keyword_net.h"
typedef struct{
	hlo_stream_t * base;
	SpeechRequest speech_pb;
	uint8_t threshold;
	uint16_t reserved;
	uint32_t timeout;
	uint8_t is_speaking;
}nn_keyword_ctx_t;

static void _voice_begin_keyword(void * ctx, Keyword_t keyword, int16_t value){
	LOGI("KEYWORD BEGIN\n");
}

bool cancel_alarm();
static void _voice_finish_keyword(void * ctx, Keyword_t keyword, int16_t value){
	nn_keyword_ctx_t * p = (nn_keyword_ctx_t *)ctx;

	p->speech_pb.has_version = true;
	p->speech_pb.version = KIT_VER;
	p->speech_pb.has_eq = true;
	p->speech_pb.eq = Equalizer_NONE;
	p->speech_pb.has_response = true;
	p->speech_pb.response = AudioFormat_MP3;
	p->speech_pb.has_sampling_rate = true;
	p->speech_pb.sampling_rate = AUDIO_SAMPLE_RATE/2;

	p->speech_pb.has_confidence = true;
	p->speech_pb.confidence = value >> 5; // I think server is expecting q7
	tinytensor_features_force_voice_activity_detection();
	p->is_speaking = true;
	p->speech_pb.has_word = true;

	switch (keyword ) {
	case okay_sense:
		LOGI("OKAY SENSE\r\n");
		LOGA("\r\nOKAY SENSE\r\n");
		p->speech_pb.word = Keyword_OK_SENSE;
		break;
	case snooze:
		p->speech_pb.word = Keyword_SNOOZE;
		break;
	case stop:
		p->speech_pb.word = Keyword_STOP;
		break;
	}
}

bool is_alarm_ringing();
static void _snooze_stop(void * ctx, Keyword_t keyword, int16_t value){
	LOGI("SNOOZE\r\n");
	if( is_alarm_ringing() && cancel_alarm() ) {
		LOGI("SNOOZING\r\n");
		_voice_finish_keyword(ctx, keyword, value);
	}
}
static void _stop_stop(void * ctx, Keyword_t keyword, int16_t value){
	LOGI("STOP\r\n");
	if( audio_playing() ) {
		LOGI("STOPPING\r\n");
		cancel_alarm();
		_voice_finish_keyword(ctx, keyword, value);
	}
}

static void _speech_detect_callback(void * context, SpeechTransition_t transition) {
	nn_keyword_ctx_t * p = (nn_keyword_ctx_t *)context;

	if (transition == start_speech) {
		DISP("start speech\r\n");
		p->is_speaking = 1;
	}

	if (transition == stop_speech) {
		p->is_speaking = 0;
		DISP("stop speech\r\n");
	}

}




extern volatile int sys_volume;
int32_t set_volume(int v, unsigned int dly);
#define AUDIO_NET_RATE (AUDIO_SAMPLE_RATE/1024)


int hlo_filter_voice_command(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
#define NSAMPLES 512
	int ret = 0;
	int16_t samples[NSAMPLES];
	uint8_t hmac[SHA1_SIZE] = {0};

	char compressed[NUM_SAMPLES_TO_RUN_FFT/2];

	adpcm_state state = (adpcm_state){0};
	bool light_open = false;

	if(!_statsMutex){
		_statsMutex = xSemaphoreCreateMutex();
		assert(_statsMutex);
		init_background_energy(StatsCallback);
	}

	static nn_keyword_ctx_t nn_ctx;
	memset(&nn_ctx, 0, sizeof(nn_ctx));

	keyword_net_initialize();
	keyword_net_register_callback(&nn_ctx,okay_sense,OKAY_SENSE_THRESHOLD,OKAY_SENSE_MIN_DURATION,_voice_begin_keyword,_voice_finish_keyword);
	keyword_net_register_callback(&nn_ctx,snooze,SNOOZE_THRESHOLD,SNOOZE_MIN_DURATION,_voice_begin_keyword,_snooze_stop);
	keyword_net_register_callback(&nn_ctx,stop,STOP_THRESHOLD,STOP_MIN_DURATION,_voice_begin_keyword,_stop_stop);
	keyword_net_register_speech_callback(&nn_ctx,_speech_detect_callback);

	//wrap output in hmac stream
	uint8_t key[AES_BLOCKSIZE];
	get_aes(key);
	hlo_stream_t * hmac_payload_str = hlo_hmac_stream(output, key, sizeof(key) );
	assert(hmac_payload_str);

	hlo_stream_t * send_str = hmac_payload_str;

	uint32_t begin = xTaskGetTickCount();
	uint32_t speech_detected_time;

	while( (ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)samples, NUM_SAMPLES_TO_RUN_FFT*2, 4)) > 0 ){
		//net always gets samples
		if( !ble_user_active() ) {
			keyword_net_add_audio_samples(samples,ret/sizeof(int16_t));
		}


		if( nn_ctx.speech_pb.has_word && nn_ctx.speech_pb.word == Keyword_OK_SENSE) {
			if( disable_voice || !wifi_status_get(HAS_IP) ) {
				LOGW("voicetrignot %d %d\n", disable_voice, wifi_status_get(HAS_IP) );
				ret = HLO_STREAM_ERROR;
				break;
			}
			if( !light_open ) {
				light_sensor_power(LOW_POWER);
				keyword_net_pause_net_operation();
				stop_led_animation(2,20);
				input = hlo_light_stream( input,true );
				send_str = hlo_stream_bw_limited( send_str, AUDIO_NET_RATE/8, 5000);
				light_open = true;

				ret = hlo_pb_encode(send_str, SpeechRequest_fields, &nn_ctx.speech_pb);
				if( ret < 0 ) {
					break;
				}

				speech_detected_time = xTaskGetTickCount();
			} else {
				adpcm_coder((short*)samples, (char*)compressed, ret / 2, &state);
				ret = hlo_stream_transfer_all(INTO_STREAM, send_str,  (uint8_t*)compressed, sizeof(compressed), 4);
				if( ret < 0 ) {
					break;
				}
			}
			if (!nn_ctx.is_speaking) {
				analytics_event("{speech_length:%d}", xTaskGetTickCount() - speech_detected_time);
				break;
			}
		} else {
			keyword_net_resume_net_operation();

			//workaround to refresh connection once time server responds
			static TickType_t _last_refresh_check = 0;
			if( xTaskGetTickCount() - _last_refresh_check > 1000 ) {
				static bool _had_ip = false;
				bool have_ip = wifi_status_get(HAS_IP);
				if( have_ip && !_had_ip ) {
					LOGI("no ip refreshing\n");
					ret = HLO_STREAM_EAGAIN;
					_had_ip = true;
					break;
				}
				_had_ip = have_ip;

				static bool _had_time = false;
				bool have_time = has_good_time();
				if( have_time && !_had_time ) {
					LOGI("no time refreshing\n");
					ret = HLO_STREAM_EAGAIN;
					_had_time = true;
					break;
				}
				_had_time = have_time;
				_last_refresh_check = xTaskGetTickCount();
			}
		}

		BREAK_ON_SIG(signal);
		if(!nn_ctx.speech_pb.has_word &&
				xTaskGetTickCount() - begin > 4*60*1000 ) {
			ret = HLO_STREAM_EAGAIN;
			break;
		}
	}
	hlo_stream_close(input);
	light_sensor_power(HIGH_POWER);

	if (ret < 0) {
		if( !disable_voice ) {
			if( ret != HLO_STREAM_EAGAIN ) {
				stop_led_animation(2, 33);
			}
			if (ret == HLO_STREAM_ERROR) {
				play_led_animation_solid(LED_MAX, LED_MAX, 0, 0, 1, 18, 1);
			}
		}
		if(output) {
			hlo_stream_close(output);
		}
	} else {
		LOGI("\r\n===========\r\n");
			LOGI("Playback Audio\r\n");
			// grab the running hmac and drop it in the stream
			get_hmac( hmac, hmac_payload_str );
			ret = hlo_stream_transfer_all(INTO_STREAM, output, hmac, sizeof(hmac), 4);

			if( ret < 0 ) {
				stop_led_animation(2, 33);
				play_led_animation_solid(LED_MAX, LED_MAX, 0, 0, 1, 18, 1);

				if(output) {
					hlo_stream_close(output);
				}
			} else {
				output = hlo_stream_bw_limited( output, 1, 5000);
				output = hlo_light_stream( output, false );

				AudioPlaybackDesc_t desc;
				desc.context = NULL;
				desc.durationInSeconds = INT32_MAX;
				desc.to_fade_out_ms = desc.fade_in_ms = desc.fade_out_ms = 0;
				desc.onFinished = NULL;
				desc.rate = AUDIO_SAMPLE_RATE;
				desc.stream = output;
				desc.volume = sys_volume;
				desc.p = hlo_filter_mp3_decoder;
				ustrncpy(desc.source_name, "voice", sizeof(desc.source_name));
				AudioTask_StartPlayback(&desc);

				LOGI("\r\n===========\r\n");
			}
	}

	hlo_stream_close(send_str);

	keyword_net_deinitialize();
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

static void _begin_keyword(void * ctx, Keyword_t keyword, int16_t value){

}
static void _finish_keyword(void * ctx, Keyword_t keyword, int16_t value){
	switch (keyword) {

		case okay_sense:
			DISP("OKAY SENSE\r\n");
			break;

		case snooze:
			DISP("SNOOZE\r\n");
			break;

		case stop:
			DISP("STOP\r\n");
			break;

		default:
			break;
		}

}
//note that filter and the stream version can not run concurrently
int hlo_filter_nn_keyword_recognition(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	int16_t samples[NUM_SAMPLES_TO_RUN_FFT];
	int ret;
	keyword_net_initialize();

	keyword_net_register_callback(0,okay_sense,OKAY_SENSE_THRESHOLD,OKAY_SENSE_MIN_DURATION, _begin_keyword,_finish_keyword);
	keyword_net_register_callback(0,snooze,SNOOZE_THRESHOLD,SNOOZE_MIN_DURATION, _begin_keyword,_finish_keyword);
	keyword_net_register_callback(0,stop,STOP_THRESHOLD,STOP_MIN_DURATION, _begin_keyword,_finish_keyword);
	//keyword_net_register_callback(0,alexa,80,_begin_keyword,_finish_keyword);

	while( (ret = hlo_stream_transfer_all(FROM_STREAM, input, (uint8_t*)samples, sizeof(samples), 4)) >= 0 ){
		keyword_net_add_audio_samples(samples,ret/sizeof(int16_t));

		hlo_stream_transfer_all(INTO_STREAM, output,  (uint8_t*)samples, ret, 4);
		BREAK_ON_SIG(signal);
	}
	DISP("Keyword Detection Exit\r\n");
	keyword_net_deinitialize();
	return 0;
}
#if 0
static int _close_nn_stream(void * ctx){
	nn_keyword_ctx_t * nn = (nn_keyword_ctx_t*)ctx;
	hlo_stream_t * base = nn->base;
	keyword_net_deinitialize();
	vPortFree(ctx);
	return hlo_stream_close(base);
}
static int _read_nn_stream(void * ctx, void * buf, size_t size){
	int ret = HLO_STREAM_ERROR;
	nn_keyword_ctx_t * nn = (nn_keyword_ctx_t*)ctx;
	if(nn->keyword_detected == 1){
		return hlo_stream_read(nn->base, buf, size);
	}else{
		ret = hlo_stream_read(nn->base, buf, size);
		if(ret < 0){
			return ret;
		}
		keyword_net_add_audio_samples(buf, ret/sizeof(int16_t));
	}
	return ret;
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
#endif
#include "mad/decoder.h"
typedef struct{
	hlo_stream_t * in;
	hlo_stream_t * out;
	hlo_stream_signal sig;
	uint32_t t_start;
	void * sig_ctx;
	uint8_t frame_buf[800];
}mp3_ctx_t;
static enum mad_flow _mp3_input(void *data, struct mad_stream *stream){
	mp3_ctx_t * ctx = (mp3_ctx_t*)data;
	if(ctx->sig && ctx->sig(ctx->sig_ctx)){
		return MAD_FLOW_STOP;
	}
	//setup default values
	uint8_t *start = ctx->frame_buf;
	int stream_bytes_to_read = sizeof(ctx->frame_buf);
	int bytes_total = 0;

	if(stream->next_frame){
		//override default values if frame is not completely consumed
		int remaining = stream->bufend - stream->next_frame;
		//DISP("c %d\r\n", remaining);
		if(remaining > 0 && remaining < sizeof(ctx->frame_buf)){
			memcpy(ctx->frame_buf, stream->next_frame, remaining);
			start = ctx->frame_buf + remaining;
			bytes_total = remaining;
			stream_bytes_to_read = sizeof(ctx->frame_buf) - remaining;
		}else{
			DISP("buffer size error\r\n");
			return MAD_FLOW_BREAK;
		}
	}else{
//		DISP("i %d\r\n", stream_bytes_to_read);
	}
	int ret = hlo_stream_transfer_all(FROM_STREAM, ctx->in, start, stream_bytes_to_read, 4);
	if(ret < 0){
		return MAD_FLOW_STOP;
	}else{
		bytes_total += ret;
	}

	mad_stream_buffer(stream, ctx->frame_buf, bytes_total);
	//vTaskDelay(100);
	return MAD_FLOW_CONTINUE;
}
static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static void _upsample( int16_t * s, int n) {
    int i;
    for(i=n-1;i!=-1;--i) {
        s[i*2]   = i == 0 ? s[i] : (s[i-1]+s[i])/2;
        s[i*2+1] = i == n-1 ? s[i] : (s[i]+s[i+1])/2;
    }
}
static
enum mad_flow _mp3_output(void *data,
             struct mad_header const *header,
             struct mad_pcm *pcm){
	//DISP("o %d\r\n", pcm->length);
	mp3_ctx_t * ctx = (mp3_ctx_t*)data;
	if(ctx->sig && ctx->sig(ctx->sig_ctx)){
		return MAD_FLOW_STOP;
	}
	int16_t * i16_samples = (int16_t*)pcm->samples[1];
	int i;
	for(i = 0; i < pcm->length; i++){
		i16_samples[i] = scale(pcm->samples[0][i]);
	}

	int ret;
	uint32_t buf_size = pcm->length * sizeof(int16_t);
	if(header)
	{
		if(header->samplerate == 16000)
		{
			_upsample(i16_samples, pcm->length);
			buf_size *= 2;
		}
		else if(header->samplerate == 32000)
		{
			// do nothing
		}
	}

	ret = hlo_stream_transfer_all(INTO_STREAM, ctx->out, (uint8_t*)i16_samples, buf_size, 4);
	if(ctx->t_start){
		analytics_event("{speech_latency: %d}", xTaskGetTickCount() - ctx->t_start);
		ctx->t_start = 0;
	}
	if( ret < 0){
		LOGE("Decode tr err %d\n", ret);
		return MAD_FLOW_BREAK;
	}
	//vTaskDelay(100);
	return MAD_FLOW_CONTINUE;
}
/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */

static
enum mad_flow _mp3_error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame){
	if(!MAD_RECOVERABLE(stream->error)){
		LOGE("MP3Error: %s\r\n", mad_stream_errorstr(stream));
		vTaskDelay(100);
		return MAD_FLOW_BREAK;
	}else{
		return MAD_FLOW_CONTINUE;
	}
}

static
enum mad_flow _mp3_header_cb(void *data,
		struct mad_header const *header){

	return MAD_FLOW_CONTINUE;
}

int hlo_filter_mp3_decoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal){
	mp3_ctx_t mp3 = {0};
	struct mad_decoder decoder;
	int result;

	/* initialize our private message structure */
	mp3.in = input;
	mp3.out = output;
	mp3.sig = signal;
	mp3.sig_ctx = ctx;
	mp3.t_start = xTaskGetTickCount();	/* log start time, set to 0 if no report */

	/* configure input, output, and error functions */

	mad_decoder_init(&decoder, &mp3,
		   _mp3_input, _mp3_header_cb, 0 /* filter */, _mp3_output,
		   _mp3_error, 0 /* message */);

	/* start decoding */

	result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

	/* release the decoder */

	mad_decoder_finish(&decoder);
	return (result == MAD_ERROR_NONE ? 0 : -1 );
}
////-----------------------------------------
//commands
static uint8_t _can_has_sig_stop(void * unused){
	return audio_sig_stop;
}
int Cmd_audio_stop(int argc, char *argv[]){
	DISP("Stopping Audio\r\n");
	audio_sig_stop = 1;
	return 0;

}
static hlo_filter _filter_from_string(const char * str){
	switch(str[0]){
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
	case 'm':
		return hlo_filter_mp3_decoder;
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


	LOGI("Cmd Stream transfer exited with code %d\r\n", ret);
	hlo_stream_close(in);
	hlo_stream_close(out);
	return 0;
}



static volatile int confidence = 70;
int cmd_confidence(int argc, char *argv[]) {
	confidence = atoi(argv[1]);
	return 0;
}
void AudioControlTask(void * unused) {
	audio_sig_stop = 0;
	int ret;

	for(;;) {

		DISP("starting new stream\n");
		audio_sig_stop = 0;

		hlo_stream_t * in;
		in = hlo_audio_open_mono(AUDIO_SAMPLE_RATE,HLO_AUDIO_RECORD);

		hlo_stream_t * out;
		char speech_url[64];
		usnprintf(speech_url, sizeof(speech_url), "https://%s%s", get_speech_server(), SPEECH_ENDPOINT );
		out = hlo_http_post(speech_url, NULL);

		if(in && out){
			ret = hlo_filter_voice_command(in,out,NULL, NULL);
		}
		LOGI("Task Stream transfer exited with code %d\r\n", ret);

		//hlo_stream_close(in);
		//hlo_stream_close(out);

		vTaskDelay(100);
	}
}

static uint8_t mic_count = 8;
static uint8_t _mic_test_stop(void * unused){

	DISP("Mic test count %d\n",mic_count);
	return (--mic_count == 0);
}



