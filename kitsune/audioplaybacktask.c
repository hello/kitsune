#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "uartstdio.h"

#include "audioplaybacktask.h"

#include "audiocapturetask.h"
#include "audioprocessingtask.h"

#include "networktask.h"

#include "network.h"
#include "mcasp_if.h"
#include "circ_buff.h"
#include "ff.h"
#include "udma.h"
#include "pcm_handler.h"
#include "i2s.h"
#include "i2c_cmd.h"
#include "udma_if.h"

#define INBOX_QUEUE_LENGTH (5)
#define AUDIO_PLAYBACK_RATE_HZ (48000)
#define NUMBER_OF_TICKS_IN_A_SECOND (1000)

#define FLAG_SUCCESS (0x01)
#define FLAG_SNOOZE  (0x02)
#define FLAG_STOP    (0x04)

/* stupid globals */
extern tCircularBuffer *pRxBuffer;
extern unsigned char g_ucSpkrStartFlag;

static xQueueHandle _queue = NULL;
static const unsigned int CPU_XDATA = 0; //1: enabled CPU interrupt triggerred, 0 for DMA

#define MAX_LOOP_ITERS (100000)

typedef enum {
	request	,
	stop,
	snooze
} EPlaybackMessage_t;

typedef struct {
	EPlaybackMessage_t type;
	union {
		AudioPlaybackDesc_t playbackinfo;
	} message;
} PlaybackMessage_t;


static void Init(void) {
	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof( PlaybackMessage_t ) );
	}
}

void AudioPlaybackTask_PlayFile(const AudioPlaybackDesc_t * playbackinfo) {

	PlaybackMessage_t m;
	memset(&m,0,sizeof(m));

	m.type = request;
	memcpy(&m.message.playbackinfo,playbackinfo,sizeof(AudioPlaybackDesc_t));


	xQueueSend(_queue,( const void * )&m,10);
}

void AudioPlaybackTask_Snooze(void) {
	PlaybackMessage_t m;
	memset(&m,0,sizeof(m));

	m.type = snooze;

	//make sure the latest interruption gets priority over other messages
	xQueueSendToFront(_queue,( const void * )&m,10);

}

void AudioPlaybackTask_StopPlayback(void) {
	PlaybackMessage_t m;
	memset(&m,0,sizeof(m));

	m.type = stop;

	UARTprintf("AudioPlaybackTask_StopPlayback\r\n");
	//make sure the latest interruption gets priority over other messages
	xQueueSendToFront(_queue,( const void * )&m,10);

}


static uint8_t CheckForInterruptionDuringPlayback(void) {
	PlaybackMessage_t m;
	uint8_t ret = 0x00;

	/* Take a peak at the top of our queue.  If we get something that says stop, we stop  */
	if (xQueuePeek(_queue,(void *)&m,0)) {

		if (m.type == stop) {
			ret = FLAG_STOP;
			UARTprintf("Stopping playback\r\n");

		}
		else if (m.type == snooze) {
			ret = FLAG_SNOOZE;
			UARTprintf("Snoozing playback\r\n");

		}

		if (ret) {
			xQueueReceive(_queue,(void *)&m,0); //pop this interruption off the queue
		}

	}

	return ret;
}

static uint8_t DoPlayback(const AudioPlaybackDesc_t * info) {

	FIL fp = {0};
	WORD size;
	FRESULT res;
	uint32_t totBytesRead = 0;
	uint32_t uiPlayWaterMark = 1;
	uint32_t iReceiveCount = 0;

	int32_t iRetVal = -1;
	uint8_t returnFlags = 0x00;

	uint16_t speaker_data_padded[512];
	uint16_t speaker_data[256];

	uint64_t loop_ticks,desired_ticks_elapsed;
	int64_t dtick;
	portTickType t1,t2;



	loop_ticks = 0;
	desired_ticks_elapsed = info->durationInSeconds * NUMBER_OF_TICKS_IN_A_SECOND;


	UARTprintf("Starting playback\r\n");
	UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);

	if (!info || !info->file) {
		UARTprintf("invalid playback info %s\n\r",info->file);
		return returnFlags;
	}

	//create circular buffer
	pRxBuffer = CreateCircularBuffer(RX_BUFFER_SIZE);

	if (!pRxBuffer) {
		UARTprintf("Failed to create circular buffer of size %d\r\n",RX_BUFFER_SIZE);
		UARTprintf("%d bytes free %d\n", xPortGetFreeHeapSize(), __LINE__);
		return returnFlags;
	}

	//zero out RX byffer
	memset(pRxBuffer->pucBufferStartPtr,0,RX_BUFFER_SIZE);

	//open file for playback
	UARTprintf("Opening %s for playback\r\n",info->file);
	res = f_open(&fp, info->file, FA_READ);

	if (res != FR_OK) {
		UARTprintf("Failed to open audio file %s\n\r",info->file);

		if (pRxBuffer) {
			DestroyCircularBuffer(pRxBuffer);
		}

		return returnFlags;
	}


	//////
	// SET UP AUDIO PLAYBACK

	get_codec_NAU(info->volume);

	// Initialize the Audio(I2S) Module
	AudioCapturerInit(CPU_XDATA, AUDIO_PLAYBACK_RATE_HZ);

	// Initialize the DMA Module
	UDMAInit();

	UDMAChannelSelect(UDMA_CH5_I2S_TX, NULL);

	// Setup the DMA Mode
	SetupPingPongDMATransferRx();

	// Setup the Audio In/Out
	AudioCapturerSetupDMAMode(DMAPingPongCompleteAppCB_opt, CB_EVENT_CONFIG_SZ);
	AudioCaptureRendererConfigure(I2S_PORT_DMA, AUDIO_PLAYBACK_RATE_HZ);

	/* FUCK!  A magic global variable here!  */
	g_ucSpkrStartFlag = 1;

	//do whatever this function does
	Audio_Start();


	memset(speaker_data_padded,0,sizeof(speaker_data_padded));

	uiPlayWaterMark = 1;



	while (1) {
		t1 = xTaskGetTickCount();

		/* Read always in block of 512 Bytes or less else it will stuck in f_read() */

		res = f_read(&fp, speaker_data, sizeof(speaker_data), &size);
		totBytesRead += size;

		/* Wait to avoid buffer overflow as reading speed is faster than playback */
		while (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
			vTaskDelay(2);
		};

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

		if (uiPlayWaterMark == 0) {
			if (IsBufferSizeFilled(pRxBuffer, PLAY_WATERMARK) == TRUE) {
				uiPlayWaterMark = 1;
			}
		}
		iReceiveCount++;

		if ((iReceiveCount % 100) == 0) {
			UARTprintf("g_iReceiveCount: %d\n\r", iReceiveCount);
			UARTprintf("%d ticks\r\n",(uint32_t)loop_ticks);

		}

		t2  = xTaskGetTickCount();

		dtick = (int64_t)t2 - (int64_t)t1;
		//watch out for rollovers of the 32 bit counter
		if (dtick < 0) {
			dtick += UINT32_MAX;
		}

		loop_ticks += dtick;

		if (loop_ticks > desired_ticks_elapsed) {
			break;
		}


		vTaskDelay(0);
	}

	///CLEANUP

	f_close(&fp);

	close_codec_NAU();
	Audio_Stop();

	/* goddamned globals */
	g_ucSpkrStartFlag = 0;

	McASPDeInit();
	DestroyCircularBuffer(pRxBuffer);

	UARTprintf("completed playback\r\n");

	returnFlags |= FLAG_SUCCESS;


	return returnFlags;

}


static void SetSnoozeWithServer(void) {

}


void AudioPlaybackTask_Thread(void * data) {
	PlaybackMessage_t m;
	uint8_t playbackResultFlags;

	Init();

	for (; ;) {
		/* Wait until we get a message */
		xQueueReceive( _queue,(void *) &m, portMAX_DELAY );

		switch (m.type) {
		case request:
		{
			AudioPlaybackDesc_t * desc = &m.message.playbackinfo;

			//disable audio capture
			AudiopProcessingTask_TurnOff();
			AudioCaptureTask_Disable();

			vTaskDelay(500); //wait for shutdown -- 1/2 second is very safe

			//do the playback
			playbackResultFlags = DoPlayback(desc);

			//enable audio capture
			AudiopProcessingTask_TurnOn();
			AudioCaptureTask_Enable();


			if (playbackResultFlags & FLAG_SNOOZE) {
				SetSnoozeWithServer();
			}

			if (desc->callback) {
				desc->callback(desc->context);
			}

			break;
		}

		case stop:
		{
			//do nothing -- this is only meaningful if it happened during playback, and it is handled during playback
			break;
		}

		case snooze:
		{
			//do nothing -- this is only meaningful if it happened during playback, and it is handled during playback
			break;
		}
		default:
		{
			UARTprintf("AudioPlaybackTask_Thread -- received message of unknown type \r\n");
			break;
		}
		}

	}
}
