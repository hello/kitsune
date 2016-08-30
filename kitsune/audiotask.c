#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "audiotask.h"

#include "network.h"
#include "audioprocessingtask.h"
#include "hellofilesystem.h"
#include "fileuploadertask.h"

#include "audiofeatures.h"
#include "uartstdio.h"
#include "endpoints.h"
#include "circ_buff.h"
#include "network.h"
#include "octogram.h"
#include "i2c_cmd.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ustdlib.h"

#include "mcasp_if.h"
#include "kit_assert.h"
#include "utils.h"

#include "adpcm.h"
#include "hlo_async.h"
#include "state.pb.h"

#include "hlo_audio.h"
#include "hlo_pipe.h"
#include "hlo_audio_tools.h"

#include "codec_debug_config.h"

#include "audiohelper.h"

#include "led_cmd.h"
#include "led_animations.h"

#if 0
#define PRINT_TIMING
#endif

#define INBOX_QUEUE_LENGTH (5)

#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP (50000)

#define MONO_BUF_LENGTH (AUDIO_FFT_SIZE) //256

#define FLAG_SUCCESS (0x01)
#define FLAG_STOP    (0x02)

/* static variables  */
static xQueueHandle _playback_queue = NULL;
static xQueueHandle _capture_queue = NULL;
static xQueueHandle _state_queue = NULL;

#ifdef KIT_INCLUDE_FILE_UPLOAD
static void QueueFileForUpload(const char * filename,uint8_t delete_after_upload) {
	FileUploaderTask_Upload(filename,DATA_SERVER,RAW_AUDIO_ENDPOINT,delete_after_upload,NULL,NULL);
}
#endif
/**
 * sense state task is an async task that is responsible for caching the correct audio playback state
 * and ensure its delivery to the server
 */
#include "endpoints.h"
#include "networktask.h"
#include "wifi_cmd.h"
extern bool encode_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static void _print_heap_info(void){
	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));
}
static SenseState sense_state;
AudioState get_audio_state() {
	return sense_state.audio_state;
}
static void _sense_state_task(hlo_future_t * result, void * ctx){
#define MAX_STATE_BACKOFF (15*60*1000)
	AudioState last_audio_state;
	sense_state.sense_id.funcs.encode = encode_device_id_string;
	sense_state.has_audio_state = true;
	bool state_sent = true;
	unsigned int backoff = 1000;
	while(1){
		if(xQueueReceive( _state_queue,(void *) &(sense_state.audio_state), backoff)){
			//reset backoff on state change
			backoff = 1000;
			last_audio_state = sense_state.audio_state;
			LOGI("AudioState %s, %s\r\n", last_audio_state.playing_audio?"Playing":"Stopped", last_audio_state.file_path);
			state_sent = NetworkTask_SendProtobuf(true, DATA_SERVER,
							SENSE_STATE_ENDPOINT, SenseState_fields, &sense_state, 0,
							NULL, NULL, NULL, true);
		}else if(!state_sent && wifi_status_get(HAS_IP)){
			sense_state.audio_state = last_audio_state;
			state_sent = NetworkTask_SendProtobuf(true, DATA_SERVER,
										SENSE_STATE_ENDPOINT, SenseState_fields, &sense_state, 0,
										NULL, NULL, NULL, true);
			backoff *= 2;
			backoff = backoff > MAX_STATE_BACKOFF ?  MAX_STATE_BACKOFF : backoff;
		}
		if( state_sent ) {
			backoff = 1000;
		}
	}
}
#include "state.pb.h"
typedef enum {
	PLAYING,
	SILENT,
} playstate_t;
static bool _queue_audio_playback_state(playstate_t is_playing, const AudioPlaybackDesc_t* info){
	AudioState ret = AudioState_init_default;
	ret.playing_audio = (is_playing == PLAYING);

	assert(!( ret.playing_audio && ( !info ) ));

	if( info  ) {
		ret.has_duration_seconds = true;
		ret.duration_seconds = info->durationInSeconds;

		ret.has_volume_percent = true;
		ret.volume_percent = info->volume * 100 / 60;

		ret.has_file_path = true;
		ustrncpy(ret.file_path, info->source_name, sizeof(ret.file_path));
	}

	return xQueueSend(_state_queue, &ret, 0);
}


static uint8_t CheckForInterruptionDuringPlayback(void * unused) {
	AudioMessage_t m;
	uint8_t ret = 0x00;

	if (xQueueReceive(_playback_queue,(void *)&m,0)) {
		xQueueSendToFront(_playback_queue, (void*)&m, 0);
		if (m.command == eAudioPlaybackStop) {
			ret = FLAG_STOP;
			LOGI("Stopping playback\r\n");
		}
		if (m.command == eAudioPlaybackStart ) {
			ret = FLAG_STOP;
//			LOGI("Switching audio\r\n");
		}
	}
	return ret;
}

extern xSemaphoreHandle i2c_smphr;

typedef struct{
	unsigned long current;
	unsigned long target;
	unsigned long ramp_up_ms;
	unsigned long ramp_down_ms;
	int32_t  duration;
}ramp_ctx_t;
extern xSemaphoreHandle i2c_smphr;

int32_t set_volume(int v, unsigned int dly);

static void _change_volume_task(hlo_future_t * result, void * ctx){
	volatile ramp_ctx_t * v = (ramp_ctx_t*)ctx;
	portTickType t0 = xTaskGetTickCount();
	while( v->target || v->current ){
		if ( (v->duration - (int32_t)(xTaskGetTickCount() - t0)) < 0 && v->duration > 0){
			v->target = 0;
		}
		if(v->current > v->target){
			vTaskDelay(v->ramp_down_ms);
			v->current--;
		}else if(v->current < v->target){
			v->current++;
			vTaskDelay(v->ramp_up_ms);
		}else{
			vTaskDelay(10);
			continue;
		}
		//fallthrough if volume adjust is needed
		if(v->current % 10 == 0){
			LOGI("Setting volume %u at %u\n", v->current, xTaskGetTickCount());
		}

		if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {
			//set vol
			vTaskDelay(5);
			set_volume(v->current, 0);
			vTaskDelay(5);
			xSemaphoreGiveRecursive(i2c_smphr);

		}else{
			//set volume failed, instantly exit out of this async worker.
			AudioTask_StopPlayback();
			hlo_future_write(result, NULL, 0, -1);
		}
	}
	AudioTask_StopPlayback();
	hlo_future_write(result, NULL, 0, 0);


}

static uint8_t fadeout_sig(void * ctx) {
	hlo_future_t * vol_task = (hlo_future_t*)ctx;

	if ( hlo_future_read(vol_task, NULL, 0, 0) != -11 ) {
		return FLAG_STOP;
	}
	return 0;
}



////-------------------------------------------
//playback sample app
static void _playback_loop(AudioPlaybackDesc_t * desc, hlo_stream_signal sig_stop){
	int ret;
	bool  vol_ramp = desc->fade_in_ms || desc->fade_out_ms || desc->to_fade_out_ms;
	ramp_ctx_t vol;
	hlo_future_t * vol_task;

	hlo_stream_t * spkr = hlo_audio_open_mono(desc->rate,HLO_AUDIO_PLAYBACK);
	hlo_stream_t * fs = desc->stream;

	if(vol_ramp) {
		vol = (ramp_ctx_t){
			.current = 0,
			.target = desc->volume,
			.ramp_up_ms = desc->fade_in_ms / (desc->volume + 1),
			.ramp_down_ms = desc->fade_out_ms / (desc->volume + 1),
			.duration =  desc->durationInSeconds * 1000,
		};
		set_volume(0, portMAX_DELAY);
		vol_task = (hlo_future_t*)hlo_future_create_task_bg(_change_volume_task,(void*)&vol,1024);
	} else {
		set_volume(desc->volume, portMAX_DELAY);
	}

	//playback
	hlo_filter transfer_function = desc->p ? desc->p : hlo_filter_data_transfer;

	ret = transfer_function(fs, spkr, desc->context, sig_stop);

	if( ret == HLO_STREAM_ERROR ) {
		play_led_animation_solid(LED_MAX, LED_MAX, 0, 0, 1,18, 1);
	}

	if( vol_ramp ) {
		//join async worker
		vol.target = 0;
		ret = transfer_function(fs, spkr, vol_task, fadeout_sig);
	}

	hlo_stream_close(fs);
	hlo_stream_close(spkr);
	if(desc->onFinished){
		desc->onFinished(desc->context);
	}
	DISP("Playback Task Finished %d\r\n", ret);
}
void AudioPlaybackTask(void * data) {
	_playback_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioMessage_t));
	assert(_playback_queue);

	_state_queue =  xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioState));
	assert(_state_queue);
	_queue_audio_playback_state(SILENT, NULL);

	hlo_future_t * state_update_task = hlo_future_create_task_bg(_sense_state_task, NULL, 1024);
	assert(state_update_task);

	while(1){
		AudioMessage_t  m;
		if (xQueueReceive( _playback_queue,(void *) &m, portMAX_DELAY )) {
			switch (m.command) {

				case eAudioPlaybackStart:
				{
					AudioPlaybackDesc_t * info = &m.message.playbackdesc;
					/** prep  **/
					_queue_audio_playback_state(PLAYING, info);
					/** blocking loop to play the sound **/
					_playback_loop(info, CheckForInterruptionDuringPlayback);
					/** clean up **/
					_queue_audio_playback_state(SILENT, info);

					if (m.message.playbackdesc.onFinished) {
						m.message.playbackdesc.onFinished(m.message.playbackdesc.context);
					}
				}   break;
				default:
					break;
			}
		}
	}

	//hlo_future_destroy(state_update_task);

}
#include "hlo_audio_tools.h"
static uint8_t CheckForInterruptionDuringCapture(void * unused){
	AudioMessage_t m;
	uint8_t ret = 0x00;
	if (xQueueReceive(_capture_queue,(void *)&m,0)) {
		ret = FLAG_STOP;
		xQueueSendToFront(_capture_queue, (void*)&m, 0);
	}
	return ret;
}
static int _do_capture(const AudioCaptureDesc_t * info){
	int ret = HLO_STREAM_ERROR;
	if(info->p){
		hlo_stream_t * mic = hlo_audio_open_mono(info->rate, HLO_AUDIO_RECORD);
		ret = info->p(mic, info->opt_out, info->ctx, CheckForInterruptionDuringCapture);
		LOGI("Capture Returned: %d\r\n", ret);
		hlo_stream_close(mic);
		if(info->flag & AUDIO_TRANSFER_FLAG_AUTO_CLOSE_OUTPUT){
			hlo_stream_close(info->opt_out);
		}
	}
	return ret;
}
#define BG_CAPTURE_TIMEOUT 2000
void AudioCaptureTask(void * data) {
	_capture_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioMessage_t));
	assert(_capture_queue);

	AudioCaptureDesc_t bg_info = {0};
	uint8_t bg_capture_enable = 0;
	for (; ;) {
		AudioMessage_t  m;
		//default
		if (xQueueReceive( _capture_queue,(void *) &m, BG_CAPTURE_TIMEOUT )) {
			switch(m.command){
			case eAudioCaptureStart:
				_do_capture(&m.message.capturedesc);
				break;
			case eAudioBGCaptureStart:
				bg_capture_enable = 1;
				bg_info = m.message.capturedesc;
				break;
			case eAudioCaptureStop:
				bg_capture_enable = 0;
				break;
			default:
				break;
			}
		}else{
			if(bg_capture_enable){
				_do_capture(&bg_info);
			}
		}
	}
}

void AudioTask_StopPlayback(void) {
	AudioMessage_t m;
	memset(&m,0,sizeof(m));

	m.command = eAudioPlaybackStop;

	//send to front of queue so this message is always processed first
	if (_playback_queue) {
		xQueueSendToFront(_playback_queue,(void *)&m,0);
		_queue_audio_playback_state(SILENT, NULL);
	}
}

void AudioTask_StartPlayback(const AudioPlaybackDesc_t * desc) {
	AudioMessage_t m;
	memset(&m,0,sizeof(m));

	m.command = eAudioPlaybackStart;

	memcpy(&m.message.playbackdesc,desc,sizeof(AudioPlaybackDesc_t));
	//send to front of queue so this message is always processed first
	if (_playback_queue) {
		xQueueSendToFront(_playback_queue,(void *)&m,0);
		_queue_audio_playback_state(PLAYING, desc);
	}

}

void AudioTask_StopCapture(void){
	AudioMessage_t m;
	m.command = eAudioCaptureStop;
	if(_capture_queue){
		xQueueSend(_capture_queue,(void *)&m,0);
	}
}

void AudioTask_StartCapture(uint32_t rate){
	AudioMessage_t m;
	m.command = eAudioBGCaptureStart;
	m.message.capturedesc.ctx = NULL;
	m.message.capturedesc.opt_out = NULL;
	m.message.capturedesc.p = hlo_filter_feature_extractor;
	m.message.capturedesc.rate = rate;
	if(_capture_queue){
		xQueueSend(_capture_queue,(void *)&m,0);
	}
}

void AudioTask_QueueCaptureProcess(const AudioCaptureDesc_t * desc){
	AudioMessage_t m;
	m.command = eAudioCaptureStart;
	memcpy(&m.message.capturedesc, desc, sizeof(*desc));
	if(_capture_queue){
		xQueueSend(_capture_queue,(void *)&m,0);
	}
}
int Cmd_AudioPlayback(int argc, char * argv[]){
	if(argc  > 1){
		AudioPlaybackDesc_t desc;
		desc.context = NULL;
		desc.durationInSeconds = 10;
		desc.fade_in_ms = 1000;
		desc.fade_out_ms = 1000;
		desc.onFinished = NULL;
		desc.rate = AUDIO_CAPTURE_PLAYBACK_RATE;
		desc.stream = fs_stream_open_media(argv[1], 0);
		desc.volume = 64;
		desc.p = hlo_filter_data_transfer;
		ustrncpy(desc.source_name, argv[1], sizeof(desc.source_name));
		AudioTask_StartPlayback(&desc);
		return 0;
	}
	return -1;
}
int Cmd_AudioCapture(int argc, char * argv[]){
	if( argc > 1 ){
		if (argv[1][0] == '0'){
			LOGI("Stopping Capture\r\n");
			AudioTask_StopCapture();
		}else{
			LOGI("Starting Capture\r\n");
			AudioTask_StartCapture(AUDIO_CAPTURE_PLAYBACK_RATE);
		}
		return 0;
	}
	LOGI("r [1|0]\r\n");
	return -1;

}

