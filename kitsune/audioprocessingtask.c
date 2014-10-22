#include "FreeRTOS.h"
#include "queue.h"

#include "audioprocessingtask.h"
#include "audioclassifier.h"

#define INBOX_QUEUE_LENGTH (3)
static xQueueHandle _queue = NULL;

static void RecordCallback(const RecordAudioRequest_t * request) {
	/* Go tell audio capture task to write to disk as it captures */
}

void AudioProcessingTask_Init(void) {
	_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof( AudioFeatureMessage_t ) );

	AudioClassifier_Init(RecordCallback,0,0);
}

void AudioProcessingTask_AddMessageToQueue(const AudioFeatureMessage_t * message) {
	xQueueSend(_queue,message,0);
}


void AudioProcessingTask_Thread(void * data) {
	AudioFeatureMessage_t message;

	for( ;; ) {
		/* Wait until we get a message */
        xQueueReceive( _queue,(void *) &message, portMAX_DELAY );

        AudioClassifier_SegmentCallback(message.feat,&message.seg);


	}
}
