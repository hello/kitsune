#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "uartstdio.h"

#include "audiotask.h"
#include "audioprocessingtask.h"
#include "audioclassifier.h"
#include "networktask.h"
#include "wifi_cmd.h"
#include "sys_time.h"

#include "kit_assert.h"

#include "matrix.pb.h"

#define INBOX_QUEUE_LENGTH (6)
static xQueueHandle _queue = NULL;
static xSemaphoreHandle _mutex = NULL;
static uint32_t samplecounter;

// each set of features is 16 bytes (representing 1 frame or 16ms) and 32 of them go into a 330 byte chunk, and we have 10 chunks, so that’s 16*32*10 = 5120ms worth of audio features
#define AUDIO_UPLOAD_PERIOD_IN_MS (5120)
#define AUDIO_UPLOAD_PERIOD_IN_TICKS (AUDIO_UPLOAD_PERIOD_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS / 2)

#define DESIRED_BUFFER_SIZE_IN_BYTES (3300) //10 chunks

static void * _longTermStorageBuffer = NULL;

typedef enum {
	command,
	features
} EAudioProcesingMessage_t;

typedef struct {
	EAudioProcesingMessage_t type;
	void * context;
	union {
		AudioFeatures_t feats;
		EAudioProcessingCommand_t cmd;
	} message;
} AudioProcessingTaskMessage_t;

static void Prepare(void * data) {
	xSemaphoreTake(_mutex,portMAX_DELAY);
}

static void Unprepare(void * data) {
	xSemaphoreGive(_mutex);
}

static void Init(void) {
	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof( AudioProcessingTaskMessage_t ) );
	}

	if (!_mutex) {
		_mutex = xSemaphoreCreateMutex();
	}

	samplecounter = 0;
	_longTermStorageBuffer = NULL;

	AudioClassifier_Init(NULL);
	AudioClassifier_SetStorageBuffers(NULL,0);

}

static void Setup(void) {

	_longTermStorageBuffer = pvPortMalloc(DESIRED_BUFFER_SIZE_IN_BYTES);

	if (_longTermStorageBuffer) {
		AudioClassifier_SetStorageBuffers(_longTermStorageBuffer,DESIRED_BUFFER_SIZE_IN_BYTES);
	}
	else {
		LOGI("Could not allocate %d bytes for audio feature storage\r\n",DESIRED_BUFFER_SIZE_IN_BYTES);
	}

}
static void TearDown(void) {

	if (_longTermStorageBuffer) {
		vPortFree(_longTermStorageBuffer);
		_longTermStorageBuffer = NULL;
		AudioClassifier_SetStorageBuffers(NULL,0);
	}
}

void AudioProcessingTask_AddFeaturesToQueue(const AudioFeatures_t * feat) {
	AudioProcessingTaskMessage_t m;
	memset(&m,0,sizeof(m));

	memcpy(&m.message.feats,feat,sizeof(AudioFeatures_t));
	m.type = features;

	if (_queue) {
		xQueueSend(_queue,&m,0);
	}
}

void AudioProcessingTask_SetControl(EAudioProcessingCommand_t cmd, TickType_t wait ) {
	AudioProcessingTaskMessage_t m;
	memset(&m,0,sizeof(m));
	m.message.cmd = cmd;
	m.type = command;

	if (_queue) {
		xQueueSend(_queue,&m,wait);
	}
}

static void NetworkResponseFunc(const NetworkResponse_t * response,
		char * reply_buf, int reply_buf_sz, void * context) {
	//LOGI("AUDIO RESPONSE:\r\n%s", reply_buf);

	if (response->success) {
    	xSemaphoreTake(_mutex,portMAX_DELAY);
		AudioClassifier_ResetStorageBuffer();
    	xSemaphoreGive(_mutex);
	}
}

static void SetUpUpload(void) {  //WARNING NONREENTRANT
	if(!has_good_time())
	{
		// This function requires a valid time
		return;
	}

	NetworkTaskServerSendMessage_t message;
	memset(&message,0,sizeof(message));

	message.host = DATA_SERVER;
	message.endpoint = AUDIO_FEATURES_ENDPOINT;
	message.response_callback = NetworkResponseFunc;
	message.retry_timeout = 0; //just try once
	message.prepare = Prepare;
	message.unprepare = Unprepare;

    message.fields = MatrixClientMessage_fields;
    message.structdata = getMatrixClientMessage();

	NetworkTask_AddMessageToQueue(&message);
}

void AudioProcessingTask_Thread(void * data) {
	AudioProcessingTaskMessage_t m;
	uint8_t featureUploads = 0;

	Init();
	Setup();
	for( ;; ) {
		/* Wait until we get a message */
        xQueueReceive( _queue,(void *) &m, portMAX_DELAY );

        //crit section begin
    	xSemaphoreTake(_mutex,portMAX_DELAY);
    	switch(m.type) {
    	case command:
    	{
    		switch (m.message.cmd) {

    		case featureUploadsOn:
    			featureUploads = 1;
    			break;
    		case featureUploadsOff:
    			featureUploads = 0;
    			break;
    		default:
    			LOGI("AudioProcessingTask_Thread -- unrecognized command\r\n");
    			break;
    		}
    		break;
    	}//end command
    	case features:
    	{
    		if (_longTermStorageBuffer) {
    			//process features
    			//indeterminate load
    			AudioClassifier_DataCallback(&m.message.feats);
    			samplecounter++;

    			//check to see if it's time to try to upload
    			if (samplecounter > AUDIO_UPLOAD_PERIOD_IN_TICKS) {
    				LOGI("uploading features\r\n");
    				if (featureUploads) {
    					SetUpUpload();
    				}
    				samplecounter = 0;
    			}
    		}

    		break;
    	}//features

    	default:
    		LOGI("AudioProcessingTask_Thread -- unrecognized message type\r\n");
    		break;
    	}
    	//end critical section
    	xSemaphoreGive(_mutex);

	}
	//TearDown();
}
