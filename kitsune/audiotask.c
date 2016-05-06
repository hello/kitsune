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
#include "audiohelper.h"
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
#if 0
#define PRINT_TIMING
#endif

#define INBOX_QUEUE_LENGTH (5)

#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP (50000)

#define MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL (5)
#define MAX_FILE_SIZE_BYTES (1048576*10)

#define FLAG_SUCCESS (0x01)
#define FLAG_STOP    (0x02)

#define SAVE_BASE "/usr/A"

TaskHandle_t audio_task_hndl;

/* static variables  */
static xQueueHandle _playback_queue = NULL;
static xQueueHandle _state_queue = NULL;
static xSemaphoreHandle _statsMutex = NULL;



static AudioOncePerMinuteData_t _stats;

static void DataCallback(const AudioFeatures_t * pfeats) {
//	LOGI("Found Feature\r\n");
	AudioProcessingTask_AddFeaturesToQueue(pfeats);
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
static void _sense_state_task(hlo_future_t * result, void * ctx){
#define MAX_STATE_BACKOFF (15*60*1000)
	SenseState sense_state;
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

		ret.has_file_path = false; //todo
		//ustrncpy(ret.file_path, info->file, sizeof(ret.file_path));
	}

	return xQueueSend(_state_queue, &ret, 0);
}



static uint8_t CheckForInterruptionDuringPlayback(void) {
	AudioMessage_t m;
	uint8_t ret = 0x00;

	/* Take a peak at the top of our queue.  If we get something that says stop, we stop  */
	if (xQueueReceive(_playback_queue,(void *)&m,0)) {

		if (m.command == eAudioPlaybackStop) {
			ret = FLAG_STOP;
			LOGI("Stopping playback\r\n");
		}
		if (!ret) {
			xQueueSendToFront(_playback_queue, (void*)&m, 0);
		}
		if (m.command == eAudioPlaybackStart ) {
			ret = FLAG_STOP;
//			LOGI("Switching audio\r\n");
		}
	}

	return ret;
}

extern xSemaphoreHandle i2c_smphr;

bool add_to_file_error_queue(char* filename, int32_t err_code, bool write_error);
typedef struct{
	unsigned long current;
	unsigned long target;
	unsigned long ramp_up_ms;
	unsigned long ramp_down_ms;
	portTickType  end_tick;
}ramp_ctx_t;
extern xSemaphoreHandle i2c_smphr;
extern bool set_volume(int v, unsigned int dly);

static void _change_volume_task(hlo_future_t * result, void * ctx){
	volatile ramp_ctx_t * v = (ramp_ctx_t*)ctx;
	while( v->target || v->current ){
		if (xTaskGetTickCount() >= v->end_tick ){
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
			hlo_future_write(result, NULL, 0, -1);
		}

	}
	hlo_future_write(result, NULL, 0, 0);
}
////-------------------------------------------
//playback sample app
static void _playback_loop(AudioPlaybackDesc_t * desc, audio_control_signal sig_stop){
	int ret;
	uint8_t chunk[512]; //arbitrary

	if(!desc || !desc->stream || !sig_stop){
		return;
	}
	ramp_ctx_t vol = (ramp_ctx_t){
		.current = 0,
		.target = desc->volume,
		.ramp_up_ms = desc->fade_in_ms / (desc->volume + 1),
		.ramp_down_ms = desc->fade_out_ms / (desc->volume + 1),
		.end_tick =  xTaskGetTickCount() + desc->durationInSeconds * 1000,
	};

	hlo_stream_t * spkr = hlo_audio_open_mono(desc->rate,0,HLO_AUDIO_PLAYBACK);
	hlo_stream_t * fs = desc->stream;

	hlo_future_t * vol_task = (hlo_future_t*)hlo_future_create_task_bg(_change_volume_task,(void*)&vol,1024);

	while( (ret = hlo_stream_transfer_between(fs,spkr,chunk, sizeof(chunk),2)) > 0){
		if( sig_stop() ){
			//signal ramp down
			vol.target = 0;
		}
		if(hlo_future_read(vol_task,NULL,0,0) >= 0){
			//future task has completed
			break;
		}
	}
	vol.target = 0;
	hlo_future_destroy(vol_task);
	hlo_stream_close(fs);
	hlo_stream_close(spkr);
	if(desc->onFinished){
		desc->onFinished(desc->context);
	}
	DISP("Playback Task Finished %d\r\n", ret);
}

static uint8_t DoPlayback(const AudioPlaybackDesc_t * info) {
	uint8_t returnFlags = 0x00;

	LOGI("Starting playback\r\n");
	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	if (!info) {
		LOGI("invalid playback info\n\r");
		_queue_audio_playback_state(SILENT, info);
		return returnFlags;
	}

	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	_queue_audio_playback_state(PLAYING, info);

	//TODO integrate ulTaskNotifyTake
	/* blocking */
	_playback_loop(info, CheckForInterruptionDuringPlayback);


	LOGI("completed playback\r\n");

	returnFlags |= FLAG_SUCCESS;

	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	_queue_audio_playback_state(SILENT, info);

	return returnFlags;

}


static uint8_t CheckForInterruptionDuringCapture(void) {
	AudioMessage_t m;
	uint8_t ret = 0x00;
	if (xQueuePeek(_playback_queue,&m,0)) {

		//pop
		xQueueReceive(_playback_queue,&m,0);

		switch (m.command) {
			case eAudioPlaybackStop:
				//fallthrough
			case eAudioPlaybackStart:
				//place message back on queue
				xQueueSendToFront(_playback_queue,&m,0);
				LOGI("received eAudioPlaybackStart\r\n");
				ret |= FLAG_STOP;
				break;
			default:
				//place message back on queue
				xQueueSendToFront(_playback_queue,&m,0);
				LOGI("audio task received message during capture that it did not understand, placing back on queue\r\n");
				break;
		}

	}
	return ret;
}

#define RECORD_SAMPLE_SIZE (256*2*2)

static void _playback_thread(hlo_future_t * result, void * ctx){
	while(1){
		AudioMessage_t  m;
		if (xQueueReceive( _playback_queue,(void *) &m, portMAX_DELAY )) {
			switch (m.command) {
				case eAudioPlaybackStart:
					DoPlayback(&(m.message.playbackdesc));
					if (m.message.playbackdesc.onFinished) {
						m.message.playbackdesc.onFinished(m.message.playbackdesc.context);
					}
					//fallthrough
				default:
					break;
			}
		}
	}
}
static void Init(void) {
	audio_task_hndl = xTaskGetCurrentTaskHandle();

	if (!_playback_queue) {
		_playback_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioMessage_t));
	}

	if (!_statsMutex) {
		_statsMutex = xSemaphoreCreateMutex();
	}

	memset(&_stats,0,sizeof(_stats));

	_state_queue =  xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioState));
	hlo_future_create_task_bg(_sense_state_task, NULL, 1024);

	_queue_audio_playback_state(SILENT, NULL);
}
void AudioTask_Thread(void * data) {

	Init();

	hlo_future_t * playback_thread = hlo_future_create_task_bg(_playback_thread, NULL, 2048);
	assert(playback_thread);

	hlo_stream_t * mic = hlo_audio_open_mono(16000,0,HLO_AUDIO_RECORD);
	assert(mic);

	uint8_t buf[RECORD_SAMPLE_SIZE] = {0};
	uint8_t stream_just_ended = 1;	/** edge transition flag **/
	int64_t callCounter = 0;		/** time context for feature extractor **/
	uint32_t settle_count = 0;		/** let audio settle after playback **/

	AudioFeatures_Init(DataCallback,StatsCallback);
	for (; ;) {
		int ret =  hlo_stream_transfer_all(FROM_STREAM, mic,buf,sizeof(buf),4);
		if( ret >= 0){
			stream_just_ended = 1;
			if(settle_count++ > 3){
				AudioFeatures_SetAudioData(buf, callCounter++);
			}
		}else if (ret == HLO_STREAM_EOF){
			DISP("EOF\r\n");
			vTaskDelay(1000);
			if(stream_just_ended){
				LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));
				stream_just_ended = 0;
				settle_count = 0;
			}
		}else{
			LOGE("MIC Stream Error: %d\r\n", ret);
			vTaskDelay(1000);
		}


	}
	hlo_future_destroy(playback_thread);
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


void AudioTask_AddMessageToQueue(const AudioMessage_t * message) {
	if (_playback_queue) {
		xQueueSend(_playback_queue,message,0);
	}
}

void AudioTask_StartCapture(uint32_t rate) {

}

void AudioTask_StopCapture(void) {

}

void AudioTask_DumpOncePerMinuteStats(AudioOncePerMinuteData_t * pdata) {
	xSemaphoreTake(_statsMutex,portMAX_DELAY);
	memcpy(pdata,&_stats,sizeof(AudioOncePerMinuteData_t));
	pdata->peak_background_energy/=pdata->num_samples;
	memset(&_stats,0,sizeof(_stats));
	xSemaphoreGive(_statsMutex);
}


