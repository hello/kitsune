#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "audiotask.h"

#include "network.h"
#include "audioprocessingtask.h"
#include "mcasp_if.h"
#include "hw_types.h"
#include "udma.h"
#include "pcm_handler.h"
#include "i2c_cmd.h"
#include "i2s.h"
#include "udma_if.h"
#include "circ_buff.h"
#include "simplelink.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "hw_ints.h"
// common interface includes
#include "common.h"
#include "hw_memmap.h"
#include "fatfs_cmd.h"
#include "ff.h"

#include "audiofeatures.h"
#include "audiohelper.h"
#include "uartstdio.h"

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

#define FLAG_SUCCESS (0x01)
#define FLAG_STOP    (0x02)

#define SAVE_FILE "/usr/POD101.raw";


/* globals */
extern tCircularBuffer *pTxBuffer;
extern tCircularBuffer *pRxBuffer;
extern unsigned int g_uiPlayWaterMark;


/* static variables  */
static xQueueHandle _queue = NULL;
static xSemaphoreHandle _processingTaskWait;

static uint8_t _isCapturing = 0;
static int64_t _callCounter;

/* CALLBACKS  */
static void ProcessingCommandFinished(void * context) {
	xSemaphoreGive(_processingTaskWait);
}

static void DataCallback(const AudioFeatures_t * pfeats) {
	AudioProcessingTask_AddFeaturesToQueue(pfeats);
}


static void Init(void) {
	_isCapturing = 0;
	_callCounter = 0;

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
			UARTprintf("Stopping playback\r\n");
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

	UARTprintf("Starting playback\r\n");
	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

	if (!info || !info->file) {
		UARTprintf("invalid playback info %s\n\r",info->file);
		return returnFlags;
	}


	if ( !InitAudioPlayback(info->volume) ) {
		UARTprintf("unable to initialize audio playback.  Probably not enough memory!\r\n");
		return returnFlags;

	}

	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

	//zero out RX byffer
	memset(pRxBuffer->pucBufferStartPtr,0,RX_BUFFER_SIZE);

	//open file for playback
	UARTprintf("Opening %s for playback\r\n",info->file);
	res = f_open(&fp, info->file, FA_READ);

	if (res != FR_OK) {
		UARTprintf("Failed to open audio file %s\n\r",info->file);
		DeinitAudioPlayback();
		return returnFlags;
	}

	memset(speaker_data_padded,0,sizeof(speaker_data_padded));

	g_uiPlayWaterMark = 1;
	t0 = xTaskGetTickCount();

	//loop until either a) done playing file for specified duration or b) our message queue gets a message that tells us to stop
	for (; ;) {

		/* Read always in block of 512 Bytes or less else it will stuck in f_read() */

		res = f_read(&fp, speaker_data, sizeof(speaker_data), &size);
		totBytesRead += size;

		/* Wait to avoid buffer overflow as reading speed is faster than playback */
		iBufWaitingCount = 0;
		while (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE &&
				iBufWaitingCount < MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL) {
			vTaskDelay(2);

			iBufWaitingCount++;

		};


		if (iBufWaitingCount >= MAX_NUMBER_TIMES_TO_WAIT_FOR_AUDIO_BUFFER_TO_FILL) {
			UARTprintf("Waited too long for audio buffer empty out to the codec \r\n");
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
				UARTprintf("Unable to fill buffer");
			}

			returnFlags |= CheckForInterruptionDuringPlayback();


			if (returnFlags) {
				//ruh-roh, gotta stop.
				UARTprintf("stopping playback");
				break;
			}

		}
		else {
			//LOOP THE FILE -- start over
			f_lseek(&fp,0);

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
	f_close(&fp);

	DeinitAudioPlayback();

	UARTprintf("completed playback\r\n");

	returnFlags |= FLAG_SUCCESS;

	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

	return returnFlags;

}





static void CaptureAudio() {
	int16_t samples[PACKET_SIZE/4];
	uint8_t * ptr_samples_bytes = (uint8_t *)&samples[0];
	uint16_t i;
	uint16_t * pu16;

	int iBufferFilled = 0;
	AudioMessage_t m;
	uint32_t num_samples_to_save;
	Filedata_t filedata;
	uint8_t isSavingToFile = 0;

#ifdef PRINT_TIMING
	uint32_t t0;
	uint32_t t1;
	uint32_t t2;
	uint32_t dt;
#endif

	AudioProcessingTask_SetControl(processingOn,ProcessingCommandFinished,NULL);

	//wait until processing is turned on
	UARTprintf("Waiting for processing to start... \r\n");
	xSemaphoreTake(_processingTaskWait,MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);
	UARTprintf("done.\r\n");

	InitAudioCapture();

	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

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

				//if you were previosly saving, close
				if (isSavingToFile) {
					CloseFile(&filedata);
					isSavingToFile = 0;
				}

				//! \todo make a file naming scheme
				memset(&filedata,0,sizeof(filedata));
				filedata.file_name = SAVE_FILE;

				InitFile(&filedata);
				isSavingToFile = 1;
				num_samples_to_save = m.message.capturedesc.captureduration;

				UARTprintf("saving %d counts to file %s\r\n",num_samples_to_save,filedata.file_name );


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
				UARTprintf("received eAudioCaptureTurnOff\r\n ");
				goto CAPTURE_CLEANUP;
			}

			case eAudioPlaybackStart:
			{
				UARTprintf("received eAudioPlaybackStart\r\n");
				goto CAPTURE_CLEANUP;
			}


			default:
			{
				//default behavior is to go to cleanup and exit function
				//so if we get audio playback turn on, we are booted out of this function
				UARTprintf("goto capture cleanup\r\n");
				goto CAPTURE_CLEANUP;
			}
			}

		}


		iBufferFilled = GetBufferSize(pTxBuffer);

		if(iBufferFilled < (sizeof(int16_t)*PACKET_SIZE)) {
			//wait a bit for the tx buffer to fill
			vTaskDelay(5);
		}
		else {
			//deinterleave (i.e. get mono)
			for (i = 0; i < PACKET_SIZE/2; i++ ) {
				ptr_samples_bytes[i] = pTxBuffer->pucReadPtr[2*i];
			}

			UpdateReadPtr(pTxBuffer, PACKET_SIZE);
#ifdef PRINT_TIMING
			t1 = xTaskGetTickCount(); dt = t1 - t0; t0 = t1;
#endif

#ifdef  SWAP_ENDIAN
			//swap endian, so we output little endian (it comes in as big endian)
			pu16 = (uint16_t *)samples;
			for (i = 0; i < PACKET_SIZE/4; i ++) {
				*pu16 = ((*pu16) << 8) | ((*pu16) >> 8);
				pu16++;
			}
#endif

			//write to file
			if (isSavingToFile) {
				WriteToFile(&filedata,PACKET_SIZE/4 * sizeof(int16_t),(const uint8_t *)samples);
				num_samples_to_save--;

				//close if we are done
				if (num_samples_to_save <= 0) {
					isSavingToFile = 0;
					CloseFile(&filedata);
				}
			}

			/* process audio to get features */
#ifdef PRINT_TIMING
			t1 = xTaskGetTickCount();
#endif
			//do audio feature processing
			AudioFeatures_SetAudioData(samples,_callCounter++);
#ifdef PRINT_TIMING
			t2 = xTaskGetTickCount();
			UARTprintf("dt = %d, compute=%d\n",dt,t2-t1); //vTaskDelay(5);
#endif
		}
	}


	CAPTURE_CLEANUP:

	if (isSavingToFile) {
		CloseFile(&filedata);
	}

	DeinitAudioCapture();

	AudioProcessingTask_SetControl(processingOff,ProcessingCommandFinished,NULL);

	//wait until ProcessingCommandFinished is returned
	xSemaphoreTake(_processingTaskWait,MAX_WAIT_TIME_FOR_PROCESSING_TO_STOP);

	UARTprintf("finished audio capture\r\n");
	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());
}





void AudioTask_Thread(void * data) {

	AudioMessage_t  m;

	Init();

	//for some reason I must have this here, or AudioFeatures_Init causes a crash.  THIS MAKES NO SENSE.

	//InitAudioCapture();
//	DeinitAudioCapture();

	//Initialize the audio features


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
				CaptureAudio();
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
	UARTprintf("Turning on audio capture\r\n");
	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOn;
	AudioTask_AddMessageToQueue(&message);

}

void AudioTask_StopCapture(void) {
	AudioMessage_t message;

	UARTprintf("Turning off audio capture\r\n");

	memset(&message,0,sizeof(message));
	message.command = eAudioCaptureTurnOff;
	AudioTask_AddMessageToQueue(&message);
}

