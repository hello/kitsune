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
#include "ustdlib.h"

#include "mcasp_if.h"
#include "kit_assert.h"

#if 0
#define PRINT_TIMING
#endif

#define INBOX_QUEUE_LENGTH (5)

#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP (500)

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
xSemaphoreHandle audio_dma_sem;

/* static variables  */
static xQueueHandle _queue = NULL;
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

	if (pdata->peak_background_energy > _stats.peak_energy) {
		_stats.peak_energy = pdata->peak_background_energy;
	}

	_stats.isValid = 1;

	xSemaphoreGive(_statsMutex);


}

static void QueueFileForUpload(const char * filename,uint8_t delete_after_upload) {
	FileUploaderTask_Upload(filename,DATA_SERVER,RAW_AUDIO_ENDPOINT,delete_after_upload,NULL,NULL);
}


static void Init(void) {
	_isCapturing = 0;
	_callCounter = 0;
	_filecounter = 0;

	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof(AudioMessage_t));
	}

	if (!_processingTaskWait) {
		_processingTaskWait = xSemaphoreCreateBinary();
	}

	if (!_statsMutex) {
		_statsMutex = xSemaphoreCreateMutex();
	}

	if( !audio_dma_sem ) {
		audio_dma_sem = xSemaphoreCreateBinary();
	}

	memset(&_stats,0,sizeof(_stats));

	AudioFeatures_Init(DataCallback,StatsCallback);

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

#include "stdbool.h"

extern void UtilsDelay(unsigned long ulCount);

//extern volatile int wait;
//extern volatile int wait2;

extern xSemaphoreHandle low_frequency_i2c_sem;

static void _lock_for_audio() {
	hello_fs_lock();
	if( low_frequency_i2c_sem ) {
		assert(xSemaphoreTake( low_frequency_i2c_sem, 60000 ));
	}
}
static void _unlock_for_audio() {
	if( low_frequency_i2c_sem ) {
		xSemaphoreGive( low_frequency_i2c_sem );
	}
	hello_fs_unlock();
}

static uint8_t DoPlayback(const AudioPlaybackDesc_t * info) {

    #define SPEAKER_DATA_CHUNK_SIZE (PING_PONG_CHUNK_SIZE)
	//1.5K on the stack
	uint16_t * speaker_data = pvPortMalloc(SPEAKER_DATA_CHUNK_SIZE);

	FIL fp = {0};
	UINT size;
	FRESULT res;
	uint32_t totBytesRead = 0;
	uint32_t iReceiveCount = 0;
	uint8_t returnFlags = 0x00;

	int32_t desired_ticks_elapsed;
	portTickType t0;

	unsigned int fade_counter=0;
	unsigned int fade_time=0;
	bool fade_in = true;
	unsigned int last_vol = 0;
	unsigned int volume = info->volume;
	unsigned int initial_volume = info->volume;
	unsigned int fade_length = info->fade_in_ms;
	bool has_fade = fade_length != 0;

	desired_ticks_elapsed = info->durationInSeconds * NUMBER_OF_TICKS_IN_A_SECOND;

	LOGI("Starting playback\r\n");
	LOGI("%d bytes free\n", xPortGetFreeHeapSize());

	_lock_for_audio();

	if (!info || !info->file) {
		_unlock_for_audio();
		LOGI("invalid playback info %s\n\r",info->file);
		vPortFree(speaker_data);
		return returnFlags;
	}
	if( has_fade ) {
		initial_volume = 1;
		t0 = fade_time = last_vol = xTaskGetTickCount();
	}

	if ( !InitAudioPlayback(initial_volume, info->rate ) ) {
		_unlock_for_audio();
		LOGI("unable to initialize audio playback.  Probably not enough memory!\r\n");
		vPortFree(speaker_data);
		return returnFlags;

	}

	LOGI("%d bytes free\n", xPortGetFreeHeapSize());


	//open file for playback
	LOGI("Opening %s for playback\r\n",info->file);
	res = hello_fs_open(&fp, info->file, FA_READ);

	if (res != FR_OK) {
		_unlock_for_audio();
		LOGI("Failed to open audio file %s\n\r",info->file);
		DeinitAudioPlayback();
		vPortFree(speaker_data);
		return returnFlags;
	}

	memset(speaker_data,0,SPEAKER_DATA_CHUNK_SIZE);

	bool started = false;
	//loop until either a) done playing file for specified duration or b) our message queue gets a message that tells us to stop
	for (; ;) {
		if( has_fade ) {
			fade_counter = xTaskGetTickCount() - fade_time;
			if( fade_counter <= fade_length && xTaskGetTickCount() - last_vol > 100 ) {
				last_vol = xTaskGetTickCount();
				if( fade_in ) {
					//UARTprintf("FI %d\n", fade_in_vol(fade_counter, volume, fade_length));
					set_volume(fade_in_vol(fade_counter, volume, fade_length),0);
				} else {
					//UARTprintf("FO %d\n", fade_out_vol(fade_counter, volume, fade_length));
					set_volume(fade_out_vol(fade_counter, volume, fade_length),0);
				}
			} else if ( !fade_in && fade_counter > fade_length ) {
				LOGI("stopping playback");
				break;
			}
		}
		/* Read always in block of 512 Bytes or less else it will stuck in hello_fs_read() */
		res = hello_fs_read(&fp, speaker_data, 512, &size);
		totBytesRead += size;

		/* Wait to avoid buffer overflow as reading speed is faster than playback */
		while ( !IsBufferVacant(pRxBuffer, 512*2) ) {
			if( !started ) {
				g_uiPlayWaterMark = 1;
				Audio_Start();
				started = true;
				break;
			}
			assert( xSemaphoreTake( audio_dma_sem, 1000 ) );
		}

		if (size > 0) {
			int i;

			for (i = (size>>1)-1; i!=-1 ; --i) {
				//the odd ones do not matter
				speaker_data[i<<1] = speaker_data[i];
				//speaker_data[(i<<1)+1] = 0;
			}

			//static volatile int slow = 250000; UtilsDelay(slow);
			FillBuffer(pRxBuffer, (unsigned char*) (speaker_data), size<<1);

			returnFlags |= CheckForInterruptionDuringPlayback();

			if( has_fade ) {
				if (returnFlags && fade_in) {
					if (fade_counter <= info->fade_in_ms) {
						//interrupted before we can get to max volume...
						volume = fade_in_vol(fade_counter, volume, fade_length);
					}
					//ruh-roh, gotta stop.
					fade_time = xTaskGetTickCount();
					fade_in = false;
					fade_length = info->fade_out_ms;
				}
			} else if( returnFlags) {
				LOGI("stopping playback");
				break;
			}

		}
		else {
			if (desired_ticks_elapsed >= 0) {
				//LOOP THE FILE -- start over
				hello_fs_lseek(&fp,0);
			}
			else {
				//someone passed in a negative number--which means after one
				//play through the file, we quit.

				break;
			}

		}

		if (g_uiPlayWaterMark == 0) {
			if (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
				g_uiPlayWaterMark = 1;
			}
		}
		iReceiveCount++;

		if (has_fade) {
			//if time elapsed is greater than desired, we quit.
			//UNLESS anddesired_ticks_elapsed is a negative number
			//instead, we will quit when the file is done playing
			if (fade_in && (xTaskGetTickCount() - t0) > desired_ticks_elapsed && desired_ticks_elapsed >= 0) {
				fade_time = xTaskGetTickCount();
				fade_in = false;
			}
		}
	}

	if( started ) {
		Audio_Stop();
	}

	vPortFree(speaker_data);

	///CLEANUP
	hello_fs_close(&fp);

	DeinitAudioPlayback();

	xSemaphoreGive( audio_dma_sem );

	_unlock_for_audio();

	LOGI("completed playback\r\n");

	returnFlags |= FLAG_SUCCESS;

	LOGI("%d bytes free\n", xPortGetFreeHeapSize());

	return returnFlags;

}





static void DoCapture(uint32_t rate) {
	int16_t * samples = pvPortMalloc(MONO_BUF_LENGTH*2*2); //256 * 2bytes * 2 = 1KB
	uint16_t i;
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

	AudioProcessingTask_SetControl(processingOn,ProcessingCommandFinished,NULL);

	//wait until processing is turned on
	LOGI("Waiting for processing to start... \r\n");
	xSemaphoreTake(_processingTaskWait,MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);
	LOGI("done.\r\n");

	InitAudioCapture(rate);

	LOGI("%d bytes free\n", xPortGetFreeHeapSize());

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
					else if (flags & AUDIO_TRANSFER_FLAG_UPLOAD) {
						const uint8_t delete_after_upload = (flags & AUDIO_TRANSFER_FLAG_DELETE_AFTER_UPLOAD) > 0;

						CloseFile(&filedata);

						QueueFileForUpload(filedata.file_name,delete_after_upload);
					}
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

		if(iBufferFilled < 2*PING_PONG_CHUNK_SIZE) {
			//wait a bit for the tx buffer to fill
			if( !xSemaphoreTake( audio_dma_sem, 1000 ) ) {
				LOGE("Capture DMA timeout\n");
				DeinitAudioCapture();
				InitAudioCapture(rate);
			}
		}
		else {
	//		uint8_ts * ptr_samples_bytes = (uint8_t *)samples;
			uint16_t * pu16 = (uint16_t *)samples;

			//dump buffer out
			ReadBuffer(pTxBuffer,(uint8_t *)samples,1024);

			for (i = 0; i < MONO_BUF_LENGTH; i++) {
				samples[i] = samples[2*i + 1];//because it goes right, left,right left, and we want the left channel.
			}

#if 1
			//swap endian, so we output little endian (it comes in as big endian)
			for (i = 0; i < MONO_BUF_LENGTH; i ++) {
				*pu16 = ((*pu16) << 8) | ((*pu16) >> 8);
				pu16++;
			}
#endif

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

	xSemaphoreGive( audio_dma_sem );

	AudioProcessingTask_SetControl(processingOff,ProcessingCommandFinished,NULL);

	//wait until ProcessingCommandFinished is returned
	xSemaphoreTake(_processingTaskWait,MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);

	LOGI("finished audio capture\r\n");
	LOGI("%d bytes free\n", xPortGetFreeHeapSize());
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
				//this will block until a message comes in
				DoCapture(m.message.capturedesc.rate);
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


