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

#include "hlo_async.h"
#include "state.pb.h"

#if 0
#define PRINT_TIMING
#endif

#define INBOX_QUEUE_LENGTH (5)

#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP (50000)

#define MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL (5)
#define MAX_FILE_SIZE_BYTES (1048576*10)

#define MONO_BUF_LENGTH (256)

#define FLAG_SUCCESS (0x01)
#define FLAG_STOP    (0x02)

#define SAVE_BASE "/usr/A"

/* globals */
unsigned int g_uiPlayWaterMark;
extern tCircularBuffer * pRxBuffer;
extern tCircularBuffer * pTxBuffer;
TaskHandle_t audio_task_hndl;

/* static variables  */
static xQueueHandle _queue = NULL;
static xQueueHandle _state_queue = NULL;
static xSemaphoreHandle _processingTaskWait = NULL;
static xSemaphoreHandle _statsMutex = NULL;

static uint8_t _isCapturing = 0;
static int64_t _callCounter;
static uint32_t _filecounter;

static AudioOncePerMinuteData_t _stats;


/* CALLBACKS  */
static void ProcessingCommandFinished(void * context) {
	xSemaphoreGive(_processingTaskWait);
}

static void DataCallback(const AudioFeatures_t * pfeats) {
	AudioProcessingTask_AddFeaturesToQueue(pfeats);
}

static void StatsCallback(const AudioOncePerMinuteData_t * pdata) {

	xSemaphoreTake(_statsMutex,portMAX_DELAY);

//	LOGI("audio disturbance: %d,  background=%d, peak=%d, samples = %d\n",pdata->num_disturbances, pdata->peak_background_energy, _stats.peak_energy , _stats.num_samples);
	_stats.num_disturbances += pdata->num_disturbances;
	_stats.num_samples++;

	_stats.peak_background_energy += pdata->peak_background_energy;

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
							NULL, NULL, NULL, false);
		}else if(!state_sent && wifi_status_get(HAS_IP)){
			sense_state.audio_state = last_audio_state;
			state_sent = NetworkTask_SendProtobuf(true, DATA_SERVER,
										SENSE_STATE_ENDPOINT, SenseState_fields, &sense_state, 0,
										NULL, NULL, NULL, false);
			backoff *= 2;
			backoff = backoff > MAX_STATE_BACKOFF ?  MAX_STATE_BACKOFF : backoff;
		}
		if( state_sent ) {
			backoff = 1000;
		}
	}
}

static void Init(void) {
	_isCapturing = 0;
	_callCounter = 0;
	_filecounter = 0;

	InitAudioHelper();

	audio_task_hndl = xTaskGetCurrentTaskHandle();

	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioMessage_t));
	}

	if (!_processingTaskWait) {
		_processingTaskWait = xSemaphoreCreateBinary();
	}

	if (!_statsMutex) {
		_statsMutex = xSemaphoreCreateMutex();
	}

	memset(&_stats,0,sizeof(_stats));
	AudioFeatures_Init(DataCallback,StatsCallback);


	_state_queue =  xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioState));
	hlo_future_create_task_bg(_sense_state_task, NULL, 1024);

}

static uint8_t CheckForInterruptionDuringPlayback(void) {
	AudioMessage_t m;
	uint8_t ret = 0x00;

	/* Take a peak at the top of our queue.  If we get something that says stop, we stop  */
	if (xQueuePeek(_queue,(void *)&m,0)) {

		if (m.command == eAudioPlaybackStop) {
			ret = FLAG_STOP;
			LOGI("Stopping playback\r\n");
		}
		if (ret) {
			xQueueReceive(_queue,(void *)&m,0); //pop this interruption off the queue
		}

	}

	return ret;
}

#define MIN_VOL 1
#define FADE_SPAN (volume - MIN_VOL)

static unsigned int fade_in_vol(unsigned int fade_counter, unsigned int volume, unsigned int fade_length) {
	return fade_counter * FADE_SPAN / fade_length + volume - FADE_SPAN;
}
static unsigned int fade_out_vol(unsigned int fade_counter, unsigned int volume, unsigned int fade_length) {
    return volume - fade_counter * FADE_SPAN / fade_length;
}
extern xSemaphoreHandle i2c_smphr;

static void _set_volume_task(hlo_future_t * result, void * ctx){
	volatile unsigned long vol_to_set = *(volatile unsigned long*)ctx;

	LOGI("Setting volume %d at %d\n", vol_to_set, xTaskGetTickCount());
	if( xSemaphoreTakeRecursive(i2c_smphr, 100)) {
		vTaskDelay(5);
		set_volume(vol_to_set, 0);
		vTaskDelay(5);
		xSemaphoreGiveRecursive(i2c_smphr);
	}
	hlo_future_write(result, NULL, 0, 0);
}
#include "state.pb.h"
static bool _queue_audio_playback_state(bool is_playing, const char * fpath, uint32_t length){
	AudioState ret = AudioState_init_default;

	ret.playing_audio = is_playing;

	ret.has_duration_seconds = true;
	ret.duration_seconds = length;

	ret.has_file_path = true;
	ustrncpy(ret.file_path, fpath, sizeof(ret.file_path));

	return xQueueSend(_state_queue, &ret, 0);
}


static uint8_t DoPlayback(const AudioPlaybackDesc_t * info) {

#define SPEAKER_DATA_CHUNK_SIZE (PING_PONG_CHUNK_SIZE)
#define FLASH_PAGE_SIZE 512

	//1.5K on the stack
	uint16_t * speaker_data = pvPortMalloc(SPEAKER_DATA_CHUNK_SIZE+1);

	FIL fp = {0};
	UINT size;
	FRESULT res;
	uint8_t returnFlags = 0x00;

	int32_t desired_ticks_elapsed;
	portTickType t0;

	static volatile unsigned long i2c_volume = 0;
	unsigned int last_set=0;
	unsigned int fade_counter=0;
	unsigned int fade_time=0;
	bool fade_in = true;
	bool fade_out = false;
	unsigned int volume = info->volume;
	unsigned int initial_volume = info->volume;
	unsigned int fade_length = info->fade_in_ms;

	desired_ticks_elapsed = info->durationInSeconds * NUMBER_OF_TICKS_IN_A_SECOND;
	LOGI("Starting playback\r\n");
	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	if (!info || !info->file) {
		LOGI("invalid playback info %s\n\r",info->file);
		vPortFree(speaker_data);
		return returnFlags;
	}

	_queue_audio_playback_state(true, info->file, info->durationInSeconds);

	if( fade_length != 0 ) {
		i2c_volume = initial_volume = 1;
		t0 = fade_time =  xTaskGetTickCount();

		LOGI("start fade in %d\n", fade_time);
	}

	if ( !InitAudioPlayback(initial_volume, info->rate ) ) {
		LOGI("unable to initialize audio playback.  Probably not enough memory!\r\n");
		vPortFree(speaker_data);
		return returnFlags;
	}
	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	//open file for playback
	LOGI("Opening %s for playback\r\n",info->file);
	res = hello_fs_open(&fp, info->file, FA_READ);

	if (res != FR_OK) {
		LOGI("Failed to open audio file %s\n\r",info->file);
		DeinitAudioPlayback();
		vPortFree(speaker_data);
		return returnFlags;
	}

	memset(speaker_data,0,SPEAKER_DATA_CHUNK_SIZE);

	bool started = false;
	//loop until either a) done playing file for specified duration or b) our message queue gets a message that tells us to stop
	for (; ;) {
		/* Read always in block of 512 Bytes or less else it will stuck in hello_fs_read() */
		res = hello_fs_read(&fp, speaker_data, FLASH_PAGE_SIZE, &size);
		if ( res == FR_OK && size > 0 ) {

			/* Wait to avoid buffer overflow as reading speed is faster than playback */
			while ( IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) ) {
				if( !started ) {
					Audio_Start();
					started = true;
					t0 = fade_time =  xTaskGetTickCount();
				}
				returnFlags |= CheckForInterruptionDuringPlayback();
				if( started && fade_length != 0  ) {
					fade_counter = xTaskGetTickCount() - fade_time;

					if (fade_counter > fade_length) {
						if( fade_in ) {
							fade_in = false;
							LOGI("finish fade in %d\n", fade_counter);
						}
						if( fade_out ) {
							LOGI("finish fade out %d\n", fade_counter);
							goto cleanup;
						}
					} else {
						int set_vol = 0;
						if (fade_in) {
							set_vol = fade_in_vol(fade_counter, volume, fade_length);
						}
						if(fade_out){
							set_vol = fade_out_vol(fade_counter, volume, fade_length);
						}
						if ( set_vol != i2c_volume && (xTaskGetTickCount() - last_set) > 100 ) {
							i2c_volume = set_vol;
							last_set = xTaskGetTickCount();
							hlo_future_destroy( hlo_future_create_task_bg(_set_volume_task, (void*)&i2c_volume, 512));
						}
					}
					if (!fade_out && (returnFlags || ((xTaskGetTickCount() - t0) > desired_ticks_elapsed && desired_ticks_elapsed > 0) ) ) {
						if (fade_in) {
							//interrupted before we can get to max volume...
							volume = fade_in_vol(fade_counter, volume, fade_length);
							fade_in = false;
						}
						//ruh-roh, gotta stop.
						fade_time = xTaskGetTickCount();
						fade_out = true;
						fade_length = info->fade_out_ms;
						fade_counter = 0;
						LOGI("start fadeout %d %d %d\n", volume, fade_length, fade_time);
					}
				} else if( returnFlags) {
					LOGI("stopping playback\n");
					goto cleanup;
				} else if( (desired_ticks_elapsed - (int32_t)(xTaskGetTickCount() - t0)) < 0 && desired_ticks_elapsed > 0 ) {
					LOGI("completed playback\n");
					goto cleanup;
				}
				if( !ulTaskNotifyTake( pdTRUE, 5000 ) ) {
					goto cleanup;
				}
			}

			FillBuffer(pRxBuffer, (unsigned char*) (speaker_data), size);
		}
		else {
			if( desired_ticks_elapsed - (xTaskGetTickCount() - t0) > 0 && desired_ticks_elapsed > 0 ) {
				//LOOP THE FILE -- start over
				LOGI("looping %d\n", desired_ticks_elapsed - (xTaskGetTickCount() - t0)  );
				hello_fs_lseek(&fp,0);
			}
			else {
				//someone passed in a negative number--which means after one
				//play through the file, we quit.
				break;
			}
		}
	}
cleanup:
	if( started ) {
		Audio_Stop();
	}

	LOGI("leftover %d\n", GetBufferSize(pRxBuffer));

	vPortFree(speaker_data);

	///CLEANUP
	DeinitAudioPlayback();

	LOGI("completed playback\r\n");

	returnFlags |= FLAG_SUCCESS;

	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	_queue_audio_playback_state(false, info->file, info->durationInSeconds);

	return returnFlags;

}





static void DoCapture(uint32_t rate) {
	int16_t * samples = pvPortMalloc(MONO_BUF_LENGTH*2*2); //256 * 2bytes * 2 = 1KB
	char filepath[32];

	int iBufferFilled = 0;
	AudioMessage_t m;
	Filedata_t filedata;
	uint8_t isSavingToFile = 0;
	uint32_t num_bytes_written;
	uint32_t octogram_count = 0;
	Octogram_t octogramdata;
	AudioOctogramDesc_t octogramdesc;
	uint32_t settle_cnt = 0;

#ifdef PRINT_TIMING
	uint32_t t0;
	uint32_t t1;
	uint32_t t2;
	uint32_t dt;
#endif

	AudioProcessingTask_SetControl(processingOn,ProcessingCommandFinished,NULL, MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);

	//wait until processing is turned on
	LOGI("Waiting for processing to start... \r\n");
	xSemaphoreTake(_processingTaskWait,MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);
	LOGI("done.\r\n");

	InitAudioCapture(rate);

	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));

	//loop until we get an event that disturbs capture
	for (; ;) {

		//poll queue as I loop through capturing
		//if a new message came in, process it, otherwise break
		//non-blocking call here
		if (xQueuePeek(_queue,&m,0)) {

			//pop
			xQueueReceive(_queue,&m,0);

			switch (m.command) {

			case  eAudioSaveToDisk:
			{

				//setup file saving...

				if (!isSavingToFile && m.message.capturedesc.change == startSaving) {
				//if you aren't already saving... make a new file
					memset(&filedata,0,sizeof(filedata));
					memset(filepath,0,sizeof(filepath));
					usnprintf(filepath,sizeof(filedata),"%s%07d.dat",SAVE_BASE,_filecounter);
					_filecounter++;

					if (_filecounter > 9999999) {
						_filecounter = 0;
					}

					filedata.file_name = filepath;

					InitFile(&filedata);
					isSavingToFile = 1;
					num_bytes_written = 0;

					LOGI("started saving to file %s\r\n",filedata.file_name );

				}
				else if (isSavingToFile && m.message.capturedesc.change == stopSaving) {
					//got message to stop saving file
					uint32_t flags = m.message.capturedesc.flags;
					isSavingToFile = 0;
					if (flags & AUDIO_TRANSFER_FLAG_DELETE_IMMEDIATELY) {
						CloseAndDeleteFile(&filedata);
					}
#ifdef KIT_INCLUDE_FILE_UPLOAD
					else if (flags & AUDIO_TRANSFER_FLAG_UPLOAD) {
						const uint8_t delete_after_upload = (flags & AUDIO_TRANSFER_FLAG_DELETE_AFTER_UPLOAD) > 0;

						CloseFile(&filedata);

						QueueFileForUpload(filedata.file_name,delete_after_upload);
					}
#endif
					else {
						//default -- just close it
						CloseFile(&filedata);
					}
				}

				break;
			}

			case eAudioCaptureTurnOn:
			case eAudioPlaybackStop:
			{
				//ignore
				break;
			}

			case eAudioCaptureOctogram:
			{

				Octogram_Init(&octogramdata);
				memcpy(&octogramdesc,&(m.message.octogramdesc),sizeof(AudioOctogramDesc_t));

				octogram_count = octogramdesc.analysisduration;

				break;
			}

			case eAudioCaptureTurnOff:
			{
				//go to cleanup, and then the next loop through
				//the thread will turn off capturing

				//place message back on queue
				xQueueSendToFront(_queue,&m,0);
				LOGI("received eAudioCaptureTurnOff\r\n ");
				goto CAPTURE_CLEANUP;
			}

			case eAudioPlaybackStart:
			{
				//place message back on queue
				xQueueSendToFront(_queue,&m,0);
				LOGI("received eAudioPlaybackStart\r\n");
				goto CAPTURE_CLEANUP;
			}

			default:
			{
				//place message back on queue
				xQueueSendToFront(_queue,&m,0);
				LOGI("audio task received message during capture that it did not understand, placing back on queue\r\n");
				break;
			}
			}

		}


		iBufferFilled = GetBufferSize(pTxBuffer);

		if(iBufferFilled < LISTEN_WATERMARK) {
			//wait a bit for the tx buffer to fill
			if( !ulTaskNotifyTake( pdTRUE, 1000 ) ) {
				LOGE("Capture DMA timeout\n");
				DeinitAudioCapture();
				InitAudioCapture(rate);
			}
		}
		else {
			//dump buffer out
			ReadBuffer(pTxBuffer,(uint8_t *)samples,PING_PONG_CHUNK_SIZE);

#ifdef PRINT_TIMING
			t1 = xTaskGetTickCount(); dt = t1 - t0; t0 = t1;
#endif

			//write to file
			if (isSavingToFile) {
				const uint32_t bytes_written = MONO_BUF_LENGTH*sizeof(int16_t);

				if (WriteToFile(&filedata,bytes_written,(const uint8_t *)samples)) {
					num_bytes_written += bytes_written;

					//close if we get too big
					if (num_bytes_written > MAX_FILE_SIZE_BYTES) {
						CloseAndDeleteFile(&filedata);
						isSavingToFile = 0;

					}
				}
				else {
					//handle the failure case
					CloseFile(&filedata);
					isSavingToFile = 0;
					LOGI("Failed to write to file %s\r\n",filedata.file_name);
					_filecounter--;
				}
			}

			/* process audio to get features */
#ifdef PRINT_TIMING
			t1 = xTaskGetTickCount();
#endif
			//do audio feature processing
			if(settle_cnt++ > 3) {
				AudioFeatures_SetAudioData(samples,_callCounter++);
				if (octogram_count > 0) {
					octogram_count--;

					Octogram_Update(&octogramdata,samples);

					if (octogram_count == 0) {
						LOGI("Finished octogram \r\n");
						Octogram_GetResult(&octogramdata,octogramdesc.result);

						if (octogramdesc.onFinished) {
							octogramdesc.onFinished(octogramdesc.context);
						}
					}
				}
			}
#ifdef PRINT_TIMING
			t2 = xTaskGetTickCount();
			LOGI("dt = %d, compute=%d\n",dt,t2-t1); //vTaskDelay(5);
#endif
		}
	}


	CAPTURE_CLEANUP:
	vPortFree(samples);

	if (isSavingToFile) {
		CloseAndDeleteFile(&filedata);
	}

	DeinitAudioCapture();

	AudioProcessingTask_SetControl(processingOff,ProcessingCommandFinished,NULL, MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);

	//wait until ProcessingCommandFinished is returned
	xSemaphoreTake(_processingTaskWait,MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);

	LOGI("finished audio capture\r\n");
	LOGI("%d free %d stk\n", xPortGetFreeHeapSize(),  uxTaskGetStackHighWaterMark(NULL));
}





void AudioTask_Thread(void * data) {

	AudioMessage_t  m;

	Init();


	for (; ;) {
		memset(&m,0,sizeof(m));

		/* Wait until we get a message */
		if (xQueueReceive( _queue,(void *) &m, portMAX_DELAY )) {


			/* PROCESS COMMANDS */
			switch (m.command) {

			case eAudioCaptureTurnOn:
			{
				//if audio was turned on, we remember that we are on
				_isCapturing = 1;

				DoCapture(m.message.capturedesc.rate);
				break;
			}

			case eAudioCaptureTurnOff:
			{
				_isCapturing = 0;
				break;
			}

			case eAudioPlaybackStart:
			{
				DoPlayback(&(m.message.playbackdesc));

				if (m.message.playbackdesc.onFinished) {
					m.message.playbackdesc.onFinished(m.message.playbackdesc.context);
				}

				break;
			}

			default:
			{
				LOGI("audio task ignoring message enum %d",m.command);
				break;
			}
			}

			//so even if we just played back a file
			//if we were supposed to be capturing, we resume that mode
			if (_isCapturing) {
				AudioTask_StartCapture(16000);
			}
		}
	}
}


void AudioTask_StopPlayback(void) {
	AudioMessage_t m;
	memset(&m,0,sizeof(m));

	m.command = eAudioPlaybackStop;

	//send to front of queue so this message is always processed first
	if (_queue) {
		xQueueSendToFront(_queue,(void *)&m,0);
	}
}

void AudioTask_StartPlayback(const AudioPlaybackDesc_t * desc) {
	AudioMessage_t m;
	memset(&m,0,sizeof(m));

	m.command = eAudioPlaybackStart;
	memcpy(&m.message.playbackdesc,desc,sizeof(AudioPlaybackDesc_t));

	//send to front of queue so this message is always processed first
	if (_queue) {
		xQueueSendToFront(_queue,(void *)&m,0);
	}
}


void AudioTask_AddMessageToQueue(const AudioMessage_t * message) {
	if (_queue) {
		xQueueSend(_queue,message,0);
	}
}

void AudioTask_StartCapture(uint32_t rate) {
	AudioMessage_t message;

	//turn on
	LOGI("mic on\r\n");
	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOn;
	message.message.capturedesc.rate = rate;
	message.message.capturedesc.flags = 0;
	AudioTask_AddMessageToQueue(&message);

}

void AudioTask_StopCapture(void) {
	AudioMessage_t message;

	LOGI("mic off\r\n");

	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOff;
	AudioTask_AddMessageToQueue(&message);
}

void AudioTask_DumpOncePerMinuteStats(AudioOncePerMinuteData_t * pdata) {
	xSemaphoreTake(_statsMutex,portMAX_DELAY);
	memcpy(pdata,&_stats,sizeof(AudioOncePerMinuteData_t));
	pdata->peak_background_energy/=pdata->num_samples;
	memset(&_stats,0,sizeof(_stats));
	xSemaphoreGive(_statsMutex);
}


