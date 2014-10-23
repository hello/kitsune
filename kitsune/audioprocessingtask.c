#include "FreeRTOS.h"
#include "queue.h"

#include "audioprocessingtask.h"
#include "audioclassifier.h"

#define INBOX_QUEUE_LENGTH (3)
static xQueueHandle _queue = NULL;

static void RecordCallback(const RecordAudioRequest_t * request) {
	/* Go tell audio capture task to write to disk as it captures */
}

static void Init(void) {
	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof( AudioFeatures_t ) );
	}

	AudioClassifier_Init(RecordCallback,0,0);
}

void AudioProcessingTask_AddFeaturesToQueue(const AudioFeatures_t * feat) {
	if (_queue) {
		xQueueSend(_queue,feat,0);
	}
}


void AudioProcessingTask_Thread(void * data) {
	AudioFeatures_t message;

	Init();

	for( ;; ) {
		/* Wait until we get a message */
        xQueueReceive( _queue,(void *) &message, portMAX_DELAY );

        AudioClassifier_DataCallback(&message);

	}
}
