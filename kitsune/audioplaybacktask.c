#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "uartstdio.h"

#include "audioplaybacktask.h"

#include "audiocapturetask.h"
#include "audioprocessingtask.h"

#include "networktask.h"

#define INBOX_QUEUE_LENGTH (5)

typedef enum {
	off,
	on
} AudioState_t;

static AudioState_t _originalCaptureState;
static AudioState_t _originalProcessingState;
static xQueueHandle _queue = NULL;

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
	xQueueSend(_queue,( const void * )&m,10);

}

void AudioPlaybackTask_StopPlayback(void) {
	PlaybackMessage_t m;
	memset(&m,0,sizeof(m));

	m.type = stop;
	xQueueSend(_queue,( const void * )&m,10);

}


static void DoPlayback(const AudioPlaybackDesc_t * info) {
	//get original state of capture and processing

	//turn off capture and processing if it was on

	//play file
}

static void StopPlayback(void) {
	//get original state of capture and processing

	//turn off capture and processing if it was on

	//play file
}

static void SetSnoozeWithServer(void) {

}


void AudioPlaybackTask_Thread(void * data) {
	PlaybackMessage_t m;

	Init();

	for (; ;) {

		/* Wait until we get a message */
		xQueueReceive( _queue,(void *) &m, portMAX_DELAY );

		switch (m.type) {
		case request:
		{
			DoPlayback(&m.message.playbackinfo);
			break;
		}

		case stop:
		{
			StopPlayback();
			break;
		}

		case snooze:
		{
			StopPlayback();
			SetSnoozeWithServer();
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
