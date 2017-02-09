#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "audiotask.h"

#include "network.h"
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
#include "hlo_audio_tools.h"
#include "codec_runtime_update.h"

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
static bool _playing = false;
bool audio_playing() {
	return _playing;
}
extern volatile int sys_volume;
extern volatile bool disable_voice;
static void _sense_state_task(hlo_future_t * result, void * ctx){
#define MAX_STATE_BACKOFF (15*60*1000)
	SenseState sense_state;

	sense_state.sense_id.funcs.encode = encode_device_id_string;
	sense_state.has_audio_state = true;
	bool state_sent = true;
	unsigned int backoff = 1000;
	while(1){
		if(xQueueReceive( _state_queue,(void *) &(sense_state.audio_state), backoff)){
			//reset backoff on state change
			backoff = 1000;
			_playing = sense_state.audio_state.playing_audio;

			sense_state.has_volume = true;
			sense_state.volume = sys_volume * 100 / 64;
			sense_state.has_voice_control_enabled = true;
			sense_state.voice_control_enabled = !disable_voice;

			LOGI("AudioState %s, %s\r\n",  _playing ?"Playing":"Stopped", sense_state.audio_state.file_path);
			state_sent = NetworkTask_SendProtobuf(true, DATA_SERVER,
							SENSE_STATE_ENDPOINT, SenseState_fields, &sense_state, 0,
							NULL, NULL, NULL, true);
		}else if(!state_sent && wifi_status_get(HAS_IP)){
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

		//TODO duplicates volume?
		ret.has_volume_percent = true;
		ret.volume_percent = info->volume * 100 / 64;

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

#define MAX_VOLUME 64

int32_t set_volume(int v, unsigned int dly);

extern volatile int16_t i2s_mon;	/* fun times */

static void _change_volume_task(hlo_future_t * result, void * ctx){
	volatile ramp_ctx_t * v = (ramp_ctx_t*)ctx;
	portTickType t0 = xTaskGetTickCount();
	int32_t count,pow;
	count = pow = 0;
	codec_runtime_prop_update(0, 2);
	while( v->target || v->current ){
		if ( (v->duration - (int32_t)(xTaskGetTickCount() - t0)) < 0 && v->duration > 0){
			v->target = 0;
		}
		if(v->current > v->target ){
			vTaskDelay(v->ramp_down_ms);
			v->current--;
			if( v->current == 0 ) {
				break;
			}
		}else if(v->current < v->target){
			v->current++;
			if( v->current > MAX_VOLUME ) {
				break;
			}
			vTaskDelay(v->ramp_up_ms);
		}else{
			vTaskDelay(10);
			pow += abs(i2s_mon);
			if (pow == 0 && count++ > 100) {
				SetAudioSignal(FILTER_SIG_RESET);
				LOGE("\r\nDAC Overflow Detected\r\n");
				break;
			} else {
				continue;
			}
		}
		//fallthrough if volume adjust is needed
		if(v->current % 10 == 0){
			LOGI("Setting volume %u at %u\n", v->current, xTaskGetTickCount());
		}

		if( xSemaphoreTakeRecursive(i2c_smphr, 10)) {
			//set vol
			vTaskDelay(5);
			set_volume(v->current, 0);
			vTaskDelay(5);
			xSemaphoreGiveRecursive(i2c_smphr);
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

extern volatile int last_set_volume;

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
		int ramp_target = desc->volume;
		if(ramp_target > 64){
			ramp_target = 64;
		}else if(ramp_target < 0){
			ramp_target = 0;
		}
		vol = (ramp_ctx_t){
			.current = 0,
			.target = ramp_target,
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

	if( vol_ramp && ret > 0 ) {
		//join async worker
		vol.target = 0;
		vol.current = last_set_volume; //handles fade out if the system volume has changed during the last playback
		ret = transfer_function(fs, spkr, vol_task, fadeout_sig);
	}

	hlo_stream_close(fs);
	hlo_stream_close(spkr);
	if(desc->onFinished){
		desc->onFinished(desc->context);
	}
	DISP("Playback Task Finished %d\r\n", ret);
}
void SetAudioSignal(int i);
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
		if (xQueueReceive( _playback_queue,(void *) &m, AUDIO_TASK_IDLE_RESET_TIME )) {
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
				}
				break;
				case eAudioResetCodec:
					LOGI("Codec Reset...");
					analytics_event("{codec: reset}");
					reset_audio();
					LOGI("done.\r\n");
					hlo_future_write(m.message.reset_sync, NULL,0, 0);
					break;
				default:
					break;
			}
		}else{
			//signal codec reset
			//this signals synchronizes the reset so that audio stream isn't being used in the meantime.
			LOGI("Codec Needs Reset\r\n");
			SetAudioSignal(FILTER_SIG_RESET);
		}
	}

	//hlo_future_destroy(state_update_task);

}
void AudioTask_ResetCodec(void) {
	AudioMessage_t m;
	memset(&m,0,sizeof(m));
	m.command = eAudioResetCodec;
	if (_playback_queue) {
		hlo_future_t * sync = hlo_future_create();
		assert(sync);
		m.message.reset_sync = sync;
		xQueueSend(_playback_queue,(void *)&m,0);
		hlo_future_read_once(sync, NULL,0);
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

int Cmd_AudioPlayback(int argc, char * argv[]){
	if(argc  > 1){
		AudioPlaybackDesc_t desc;
		desc.context = NULL;
		desc.durationInSeconds = 10;
		desc.fade_in_ms = 1000;
		desc.fade_out_ms = 1000;
		desc.onFinished = NULL;
		desc.rate = AUDIO_SAMPLE_RATE;
		desc.stream = fs_stream_open_media(argv[1], 0);
		desc.volume = 64;
		desc.p = hlo_filter_data_transfer;
		ustrncpy(desc.source_name, argv[1], sizeof(desc.source_name));
		AudioTask_StartPlayback(&desc);
		return 0;
	}
	return -1;
}
