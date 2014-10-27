#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"

#include "audiocapturetask.h"
#include "audioprocessingtask.h"
#include "audioclassifier.h"
#include "networktask.h"

#define INBOX_QUEUE_LENGTH (6)
#define UPLOAD_TIMER_PERIOD_IN_TICKS (60000)
static xQueueHandle _queue = NULL;
static TimerHandle_t _uploadtimer = NULL;
static uint8_t _decodebuf[256];

static const char * k_audio_endopoint = "audio_features";

/*  Add a message to the queue */
static void UploadTimerCallback( TimerHandle_t pxTimer ) {
	AudioProcessingTaskMessage_t message;
	message.type = eTimeToUpload;
	message.payload.empty = 0;

	AudioProcessingTask_AddFeaturesToQueue(&message);
}


static void RecordCallback(const RecordAudioRequest_t * request) {
	/* Go tell audio capture task to write to disk as it captures */
	AudioCaptureMessage_t message;
	memset(&message,0,sizeof(message));

	message.captureduration = request->durationInFrames;
	message.command = eAudioCaptureSaveToDisk;

	AudioCaptureTask_AddMessageToQueue(&message);
}

static void Init(void) {
	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof( AudioProcessingTaskMessage_t ) );
	}

	if (!_uploadtimer) {
		_uploadtimer = xTimerCreate("audioUploadTimer",UPLOAD_TIMER_PERIOD_IN_TICKS,pdTRUE,"AudioProcessingTaskTimer",UploadTimerCallback);
	}

	AudioClassifier_Init(RecordCallback,0,0);
}

void AudioProcessingTask_AddFeaturesToQueue(const AudioProcessingTaskMessage_t * feat) {
	if (_queue) {

		xQueueSend(_queue,feat,0);
	}
}

static void NetworkResponseFunc(const NetworkResponse_t * response) {

}

static void SetUpUpload(void) {
	NetworkTaskServerSendMessage_t message;
	memset(&message,0,sizeof(message));

	message.decode_buf = _decodebuf;
	message.decode_buf_size = sizeof(_decodebuf);

	message.endpoint = k_audio_endopoint;
	message.response_callback = NetworkResponseFunc;
	message.retry_timeout = 8000;

	message.encode = AudioClassifier_EncodeAudioFeatures;

	NetworkTask_AddMessageToQueue(&message);
}

void AudioProcessingTask_Thread(void * data) {
	AudioProcessingTaskMessage_t message;

	Init();

	for( ;; ) {
		/* Wait until we get a message */
        xQueueReceive( _queue,(void *) &message, portMAX_DELAY );

        switch (message.type) {
        case eFeats:
        {
            AudioClassifier_DataCallback(&message.payload.feats);
        	break;
        }

        case eTimeToUpload:
        {
        	//set up callback with network task
        	SetUpUpload();
        	break;
        }

        default:
        {
        	break;
        }
        }




	}
}
