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

#include <string.h>
#include <stdio.h>

#define SWAP_ENDIAN

#if 0
#define PRINT_TIMING
#endif

#define INBOX_QUEUE_LENGTH (5)

#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define LOOP_DELAY_WHILE_PROCESSING_IN_TICKS (1)
#define LOOP_DELAY_WHILE_WAITING_BUFFER_TO_FILL (5)

#define MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP (500)

#define MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL (5000)
#define MAX_FILE_SIZE_BYTES (1048576)

#define MONO_BUF_LENGTH (256)

#define FLAG_SUCCESS (0x01)
#define FLAG_STOP    (0x02)

#define SAVE_BASE "/usr/A"

/* globals */
unsigned int g_uiPlayWaterMark;
extern tCircularBuffer * pRxBuffer;
extern tCircularBuffer * pTxBuffer;

/* static variables  */
static xQueueHandle _queue = NULL;
static xSemaphoreHandle _processingTaskWait;

static uint8_t _isCapturing = 0;
static int64_t _callCounter;
static uint32_t _filecounter;

/* CALLBACKS  */
static void ProcessingCommandFinished(void * context) {
	xSemaphoreGive(_processingTaskWait);
}

static void DataCallback(const AudioFeatures_t * pfeats) {
	AudioProcessingTask_AddFeaturesToQueue(pfeats);
}

static void QueueFileForUpload(const char * filename) {
	FileUploaderTask_Upload(filename,DATA_SERVER,RAW_AUDIO_ENDOPOINT,1,NULL,NULL);
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

	AudioFeatures_Init(DataCallback);

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

static uint8_t DoPlayback(const AudioPlaybackDesc_t * info) {

	//1.5K on the stack
	uint16_t speaker_data_padded[512];
	uint16_t speaker_data[256];


	FIL fp = {0};
	WORD size;
	FRESULT res;
	uint32_t totBytesRead = 0;
	uint32_t iReceiveCount = 0;

	int32_t iRetVal = -1;
	uint8_t returnFlags = 0x00;

	uint32_t desired_ticks_elapsed;
	portTickType t0;
	uint32_t iBufWaitingCount;

	desired_ticks_elapsed = info->durationInSeconds * NUMBER_OF_TICKS_IN_A_SECOND;

	LOGI("Starting playback\r\n");
	LOGI("%d bytes free\n", xPortGetFreeHeapSize());

	if (!info || !info->file) {
		LOGI("invalid playback info %s\n\r",info->file);
		return returnFlags;
	}


	if ( !InitAudioPlayback(info->volume) ) {
		LOGI("unable to initialize audio playback.  Probably not enough memory!\r\n");
		return returnFlags;

	}

	LOGI("%d bytes free\n", xPortGetFreeHeapSize());


	//open file for playback
	LOGI("Opening %s for playback\r\n",info->file);
	res = hello_fs_open(&fp, info->file, FA_READ);

	if (res != FR_OK) {
		LOGI("Failed to open audio file %s\n\r",info->file);
		DeinitAudioPlayback();
		return returnFlags;
	}

	memset(speaker_data_padded,0,sizeof(speaker_data_padded));

	g_uiPlayWaterMark = 1;
	t0 = xTaskGetTickCount();

	//loop until either a) done playing file for specified duration or b) our message queue gets a message that tells us to stop
	for (; ;) {

		/* Read always in block of 512 Bytes or less else it will stuck in hello_fs_read() */

		res = hello_fs_read(&fp, speaker_data, sizeof(speaker_data), &size);
		totBytesRead += size;

		/* Wait to avoid buffer overflow as reading speed is faster than playback */
		iBufWaitingCount = 0;
		while (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE &&
				iBufWaitingCount < MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL) {
			vTaskDelay(2);

			iBufWaitingCount++;

		};


		if (iBufWaitingCount >= MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL) {
			LOGI("Waited too long for audio buffer empty out to the codec \r\n");
			break;
		}


		if (size > 0) {
			unsigned int i;

			for (i = 0; i != (size>>1); ++i) {
				//the odd ones are zeroed already
				speaker_data_padded[i<<1] = speaker_data[i];
			}

			iRetVal = FillBuffer(pRxBuffer, (unsigned char*) (speaker_data_padded), size<<1);

			if (iRetVal < 0) {
				LOGI("Unable to fill buffer");
			}

			returnFlags |= CheckForInterruptionDuringPlayback();


			if (returnFlags) {
				//ruh-roh, gotta stop.
				LOGI("stopping playback");
				break;
			}

		}
		else {
			//LOOP THE FILE -- start over
			hello_fs_lseek(&fp,0);

		}

		if (g_uiPlayWaterMark == 0) {
			if (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
				g_uiPlayWaterMark = 1;
			}
		}
		iReceiveCount++;


		if ( (xTaskGetTickCount() - t0) > desired_ticks_elapsed) {
			break;
		}


		vTaskDelay(0);
	}

	///CLEANUP
	hello_fs_close(&fp);

	DeinitAudioPlayback();

	LOGI("completed playback\r\n");

	returnFlags |= FLAG_SUCCESS;

	LOGI("%d bytes free\n", xPortGetFreeHeapSize());

	return returnFlags;

}



#define HISTORY_BUFFER_2N (12)
#define HISTORY_BUFFER_NUM_ELEMENTS (1 << HISTORY_BUFFER_2N)
#define HISTORY_BUFFER_SIZE (HISTORY_BUFFER_NUM_ELEMENTS * sizeof(int16_t))
#define HISTORY_BUF_MASK (HISTORY_BUFFER_NUM_ELEMENTS - 1)

static void DoCapture() {
	int16_t samples[MONO_BUF_LENGTH]; //512 bytes
	uint16_t i;
	char filepath[32];

	int iBufferFilled = 0;
	AudioMessage_t m;
	uint32_t num_samples_to_save;
	Filedata_t filedata;
	uint8_t isSavingToFile = 0;
	uint32_t num_bytes_written;
	uint32_t history_buf_idx = 0;
	uint8_t isHistoryNeedSaving = 0;

	int16_t * history_buffer = NULL;

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

	InitAudioCapture();

	history_buffer = (int16_t *) pvPortMalloc(HISTORY_BUFFER_SIZE);

	if (!history_buffer) {
		UARTprintf("audio capture failed to allocate history buffer\r\n");
		goto CAPTURE_CLEANUP;
	}

	memset(history_buffer,0,HISTORY_BUFFER_SIZE);

	LOGI("%d bytes free\n", xPortGetFreeHeapSize());

	//loop until we get an event that disturbs capture
	for (; ;) {

		//poll queue as I loop through capturing
		//if a new message came in, process it, otherwise break
		//non-blocking call here
		if (xQueuePeek(_queue,&m,0)) {

			switch (m.command) {

			case  eAudioSaveToDisk:
			{

				//setup file saving...
				//pop
				xQueueReceive(_queue,&m,0);

				//if you aren't already saving... make a new file
				if (!isSavingToFile) {
					memset(&filedata,0,sizeof(filedata));
					memset(filepath,0,sizeof(filepath));
					snprintf(filepath,sizeof(filedata),"%s%07d.dat",SAVE_BASE,_filecounter);
					_filecounter++;


					filedata.file_name = filepath;

					InitFile(&filedata);
					isSavingToFile = 1;
					isHistoryNeedSaving = 1;
					num_bytes_written = 0;

					LOGI("started saving to file %s\r\n",filedata.file_name );

				}

				//no matter what, set num_samples_to_save to the requested
				num_samples_to_save = m.message.capturedesc.captureduration;

				break;
			}

			case eAudioCaptureTurnOn:
			case eAudioPlaybackStop:
			{
				//ignore and remove
				xQueueReceive(_queue,&m,0);
				break;
			}

			case eAudioCaptureTurnOff:
			{
				//go to cleanup, and then the next loop through
				//the thread will turn off capturing
				LOGI("received eAudioCaptureTurnOff\r\n ");
				goto CAPTURE_CLEANUP;
			}

			case eAudioPlaybackStart:
			{
				LOGI("received eAudioPlaybackStart\r\n");
				goto CAPTURE_CLEANUP;
			}


			default:
			{
				//default behavior is to go to cleanup and exit function
				//so if we get audio playback turn on, we are booted out of this function
				LOGI("goto capture cleanup\r\n");
				goto CAPTURE_CLEANUP;
			}
			}

		}


		iBufferFilled = GetBufferSize(pTxBuffer);

		//data is in "stereo", so a filled buffer is 1kB, MONO_BUF_LENGTH * 2 channels * sizeof(int16_t)
		if(iBufferFilled <= MONO_BUF_LENGTH*sizeof(int16_t)*2) {
			//wait a bit for the tx buffer to fill
			vTaskDelay(5);
		}
		else {
			int16_t * shortBufPtr = (int16_t*) (pTxBuffer->pucReadPtr+1);
			//deinterleave (i.e. get mono)
			for (i = 0; i < MONO_BUF_LENGTH; i++ ) {
				samples[i] = shortBufPtr[2*i];
			}

			UpdateReadPtr(pTxBuffer, MONO_BUF_LENGTH*sizeof(int16_t)*2);
#ifdef PRINT_TIMING
			t1 = xTaskGetTickCount(); dt = t1 - t0; t0 = t1;
#endif

			//write to file
			if (isSavingToFile) {
				const uint32_t bytes_written = MONO_BUF_LENGTH * sizeof(int16_t);

				if (isHistoryNeedSaving) {
					int16_t * part1 = history_buffer + history_buf_idx;
					int16_t * part2 = history_buffer;

					WriteToFile(&filedata, (HISTORY_BUFFER_NUM_ELEMENTS - history_buf_idx)*sizeof(int16_t),(const uint8_t *)part1);
					WriteToFile(&filedata, history_buf_idx*sizeof(int16_t),(const uint8_t *)part2);

					isHistoryNeedSaving = 0;
				}


				if (WriteToFile(&filedata,bytes_written,(const uint8_t *)samples)) {
					num_bytes_written += bytes_written;
					num_samples_to_save--;

					//close if we are done
					if (num_samples_to_save <= 0 || num_bytes_written > MAX_FILE_SIZE_BYTES) {
						CloseFile(&filedata);
						isSavingToFile = 0;
						QueueFileForUpload(filedata.file_name);
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


			memcpy(history_buffer + history_buf_idx,samples,MONO_BUF_LENGTH*sizeof(int16_t));
			history_buf_idx += MONO_BUF_LENGTH;
			history_buf_idx &= HISTORY_BUF_MASK;

			/* process audio to get features */
#ifdef PRINT_TIMING
			t1 = xTaskGetTickCount();
#endif
			//do audio feature processing
			AudioFeatures_SetAudioData(samples,_callCounter++);
#ifdef PRINT_TIMING
			t2 = xTaskGetTickCount();
			LOGI("dt = %d, compute=%d\n",dt,t2-t1); //vTaskDelay(5);
#endif
		}
	}


	CAPTURE_CLEANUP:

	if (isSavingToFile) {
		CloseFile(&filedata);
		QueueFileForUpload(filedata.file_name);
	}

	DeinitAudioCapture();

	if (history_buffer) {
		vPortFree(history_buffer);
	}

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
				break;
			}
			}



			//so even if we just played back a file
			//if we were supposed to be capturing, we resume that mode
			if (_isCapturing) {
				//this will block until a message comes in
				DoCapture();
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

void AudioTask_StartCapture(void) {
	AudioMessage_t message;

	//turn on
	LOGI("Turning on audio capture\r\n");
	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOn;
	AudioTask_AddMessageToQueue(&message);

}

void AudioTask_StopCapture(void) {
	AudioMessage_t message;

	LOGI("Turning off audio capture\r\n");

	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOff;
	AudioTask_AddMessageToQueue(&message);
}

