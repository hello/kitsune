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

#if 0
#define PRINT_TIMING
#endif

#define INBOX_QUEUE_LENGTH (5)

#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP (50000)

#define MONO_BUF_LENGTH (AUDIO_FFT_SIZE) //256

#define FLAG_SUCCESS     (0x01)
#define FLAG_STOP        (0x02)
#define FLAG_INTERRUPTED (0x03)

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
			ret = FLAG_INTERRUPTED;
//			LOGI("Switching audio\r\n");
		}
	}
	return ret;
}

extern xSemaphoreHandle i2c_smphr;

typedef struct{
	long current;
	long target;
	unsigned long ramp_up_ms;
	unsigned long ramp_down_ms;
	int32_t  duration;
}ramp_ctx_t;

int32_t set_volume(int v, unsigned int dly);
int32_t reduce_volume( int v, unsigned int dly );

static void _change_volume_task(hlo_future_t * result, void * ctx){
	xQueueHandle volume_queue =  (xQueueHandle)ctx;
	ramp_ctx_t v;
	xQueueReceive( volume_queue,(void *) &v, portMAX_DELAY );

	long last_set_vol = 0;
	portTickType t0 = xTaskGetTickCount();
	portTickType last_set = 0;
	while( v.target != 0 || v.current != 0 ){
		if ( (v.duration - (int32_t)(xTaskGetTickCount() - t0)) < 0 && v.duration > 0){
			v.target = 0;
		}
		if(v.current > v.target){
			vTaskDelay(v.ramp_down_ms);
			v.current--;
		}else if(v.current < v.target){
			v.current++;
			vTaskDelay(v.ramp_up_ms);
		}
		if((v.target<0) || (v.current<0)) {
			break;
		}

		if( ( v.current != last_set_vol ) && ( xTaskGetTickCount() - last_set > 20 ) ) {
			DISP("%u %u at %u\n", v.current, v.target, xTaskGetTickCount());
			set_volume(v.current, 0);
			last_set_vol = v.current;
			last_set = xTaskGetTickCount();
		} else {
			vTaskDelay(2);
		}

		TickType_t wait_for_msgs = 0;
		if(v.current == v.target && (v.current != 0) ) {
			wait_for_msgs = portMAX_DELAY;
		}
		xQueueReceive( volume_queue,(void *) &v, wait_for_msgs );
	}
	set_volume(v.current<0?0:v.current, 100);
	hlo_future_write(result, NULL, 0, 0);
}

static uint8_t fadeout_sig(void * ctx) {
	hlo_future_t * vol_task = (hlo_future_t*)ctx;

	if ( hlo_future_read(vol_task, NULL, 0, 0) != -11 ) {
		return FLAG_STOP;
	}
	return 0;
}

extern volatile int last_fadeout_volume;

////-------------------------------------------
//playback sample app
static int _playback_loop(AudioPlaybackDesc_t * desc, hlo_stream_signal sig_stop){
	int main_ret=FLAG_STOP,ret=FLAG_STOP;
	bool  vol_ramp = desc->fade_in_ms || desc->fade_out_ms || desc->to_fade_out_ms;
	ramp_ctx_t vol;
	hlo_future_t * vol_task;
	xQueueHandle volume_queue = NULL;

	hlo_stream_t * spkr = hlo_audio_open_mono(desc->rate,HLO_AUDIO_PLAYBACK);
	hlo_stream_t * fs = desc->stream;

	if(vol_ramp) {
		volume_queue = xQueueCreate(1,sizeof(ramp_ctx_t));
		int ramp_target = desc->volume;
		if(ramp_target > 64){
			ramp_target = 64;
		}else if(ramp_target < 0){
			ramp_target = 0;
		}
		set_volume(0, portMAX_DELAY);

		vol = (ramp_ctx_t){
			.current = 0,
			.target = ramp_target,
			.ramp_up_ms = desc->fade_in_ms / (desc->volume + 1),
			.ramp_down_ms = desc->fade_out_ms / (desc->volume + 1),
			.duration =  desc->durationInSeconds * 1000,
		};
		xQueueSend(volume_queue, &vol, portMAX_DELAY);
		vol_task = (hlo_future_t*)hlo_future_create_task_bg(_change_volume_task,(void*)volume_queue,2048);
	} else {
		set_volume(desc->volume, portMAX_DELAY);
	}

	//playback
	hlo_filter transfer_function = desc->p ? desc->p : hlo_filter_data_transfer;

	main_ret = transfer_function(fs, spkr, desc->context, sig_stop);

	if( vol_ramp ) {
		vTaskDelay(1000*RX_BUFFER_SIZE/(2*AUDIO_SAMPLE_RATE) - 200 ); //wait for buffer to almost drain...
		//join async worker
		vol.target = 0;
		vol.current = last_fadeout_volume; //handles fade out if the system volume has changed during the last playback
		xQueueSend(volume_queue, &vol, portMAX_DELAY);

		if( main_ret > 0 ) {
			ret = transfer_function(fs, spkr, vol_task, fadeout_sig);
			if( ret != FLAG_STOP ) {
				hlo_future_read_once(vol_task, NULL, 0);
			}
			hlo_future_destroy(vol_task);
		} else {
			hlo_future_read_once(vol_task, NULL, 0);
		}
		vQueueDelete(volume_queue);
	}

	if( main_ret != FLAG_INTERRUPTED ) {
		hlo_stream_close(fs);
		if(desc->onFinished){
			desc->onFinished(desc->context);
		}
	}
	hlo_stream_close(spkr);
	DISP("Playback Task Finished %d, %d\r\n", main_ret, ret);

	return main_ret;
}

void AudioPlaybackTask(void * data) {
	AudioMessage_t m_resume;
	m_resume.command = eAudioPlaybackStop;

	_playback_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioMessage_t));
	assert(_playback_queue);

	_state_queue =  xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioState));
	assert(_state_queue);
	_queue_audio_playback_state(SILENT, NULL);

	hlo_future_t * state_update_task = hlo_future_create_task_bg(_sense_state_task, NULL, 1024);
	assert(state_update_task);

	int intdepth = 0;

	while(1){
		AudioMessage_t  m;

		if (xQueueReceive( _playback_queue,(void *) &m, portMAX_DELAY )) {
			top:

			switch (m.command) {

				case eAudioPlaybackStart:
				{
					int r;
					AudioPlaybackDesc_t * info = &m.message.playbackdesc;
					/** prep  **/
					_queue_audio_playback_state(PLAYING, info);

					if( info->onPlay ) {
						info->onPlay(info->context);
					}

					/** blocking loop to play the sound **/
					r = _playback_loop(info, CheckForInterruptionDuringPlayback);

					if( r == FLAG_STOP) {
						/** clean up **/
						_queue_audio_playback_state(SILENT, info);
					}

					if( r == FLAG_INTERRUPTED ) {
						if( intdepth == 0 ) {
							DISP("interrupted %d\n\n\n", intdepth);
							if(info->onInterrupt) {
								info->onInterrupt(info->context);
							}
							m_resume = m;
							intdepth++;
						} else {
							hlo_stream_close(info->stream);
							if(info->onFinished){
								info->onFinished(info->context);
							}
						}
					} else if( m_resume.command != eAudioPlaybackStop) {
						DISP("resumed %d\n\n\n", intdepth);
						m = m_resume;
						m_resume.command = eAudioPlaybackStop;
						intdepth--;
						goto top;
					}
				}
				break;
				case eAudioResetCodec:
					LOGI("Codec Reset...");
					reset_audio();
					LOGI("done.\r\n");
					break;
				default:
					break;
			}
		}
	}

	//hlo_future_destroy(state_update_task);

}
void AudioTask_ResetCodec(void) {
	AudioMessage_t m;
	memset(&m,0,sizeof(m));
	m.command = eAudioResetCodec;
	if (_playback_queue) {
		xQueueSend(_playback_queue,(void *)&m,0);
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
		desc.onFinished = desc.onPlay = desc.onInterrupt = NULL;
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
int cmd_codec_reset(int argc, char * argv[]){
	AudioTask_ResetCodec();
	return 0;
}

