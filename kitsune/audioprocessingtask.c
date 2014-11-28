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

#define INBOX_QUEUE_LENGTH (6)
static xQueueHandle _queue = NULL;
static xSemaphoreHandle _mutex = NULL;
static uint8_t _decodebuf[1024];
static uint32_t samplecounter;

#define AUDIO_UPLOAD_PERIOD_IN_MS (60000)
#define AUDIO_UPLOAD_PERIOD_IN_TICKS (AUDIO_UPLOAD_PERIOD_IN_MS / SAMPLE_PERIOD_IN_MILLISECONDS / 2)

#define DESIRED_BUFFER_SIZE_IN_BYTES (20000)

static DeviceCurrentInfo_t _deviceCurrentInfo;
static void * _longTermStorageBuffer = NULL;

typedef enum {
	command,
	features
} EAudioProcesingMessage_t;

typedef struct {
	EAudioProcesingMessage_t type;
	NotificationCallback_t onFinished;
	void * context;

	union {
		AudioFeatures_t feats;
		EAudioProcessingCommand_t cmd;
	} message;
} AudioProcessingTaskMessage_t;


static void RecordCallback(const RecordAudioRequest_t * request) {
	/* Go tell audio capture task to write to disk as it captures */
	AudioMessage_t m;
	memset(&m,0,sizeof(m));

	m.message.capturedesc.captureduration = request->durationInFrames;
	m.command = eAudioSaveToDisk;

	AudioTask_AddMessageToQueue(&m);
}

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

	AudioClassifier_Init(RecordCallback);
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

void AudioProcessingTask_SetControl(EAudioProcessingCommand_t cmd,NotificationCallback_t onFinished, void * context) {
	AudioProcessingTaskMessage_t m;
	memset(&m,0,sizeof(m));
	m.message.cmd = cmd;
	m.type = command;
	m.onFinished = onFinished;
	m.context = context;

	if (_queue) {
		xQueueSend(_queue,&m,0);
	}
}

static void NetworkResponseFunc(const NetworkResponse_t * response,void * context) {
	LOGI("AUDIO RESPONSE:\r\n%s",_decodebuf);

	if (response->success) {
    	xSemaphoreTake(_mutex,portMAX_DELAY);
		AudioClassifier_ResetStorageBuffer();
    	xSemaphoreGive(_mutex);
	}
}

static void SetUpUpload(void) {
	if(!has_good_time())
	{
		// This function requires a valid time
		return;
	}

	NetworkTaskServerSendMessage_t message;
	memset(&message,0,sizeof(message));
	memset(_decodebuf,0,sizeof(_decodebuf));

	message.decode_buf = _decodebuf;
	message.decode_buf_size = sizeof(_decodebuf);

	message.host = DATA_SERVER;
	message.endpoint = AUDIO_FEATURES_ENDPOINT;
	message.response_callback = NetworkResponseFunc;
	message.retry_timeout = 0; //just try once
	message.prepare = Prepare;
	message.unprepare = Unprepare;

	message.encode = AudioClassifier_EncodeAudioFeatures;

	get_mac(_deviceCurrentInfo.mac);
	_deviceCurrentInfo.unix_time = get_time();

	message.encodedata = (void *) &_deviceCurrentInfo;

	NetworkTask_AddMessageToQueue(&message);
}

void AudioProcessingTask_Thread(void * data) {
	AudioProcessingTaskMessage_t m;

	Init();

	for( ;; ) {
		/* Wait until we get a message */
        xQueueReceive( _queue,(void *) &m, portMAX_DELAY );

        //crit section begin
    	xSemaphoreTake(_mutex,portMAX_DELAY);
    	switch(m.type) {
    	case command:
    	{
    		switch (m.message.cmd) {

    		case processingOff:
    		{
    			TearDown();
    			break;
    		}

    		case processingOn:
    		{
    			Setup();
    			break;
    		}

    		default:
    		{
    			LOGI("AudioProcessingTask_Thread -- unrecognized command\r\n");
    			break;
    		}
    		}

    		break;
    	}

    	case features:
    	{
    		if (_longTermStorageBuffer) {
    			//process features
    			AudioClassifier_DataCallback(&m.message.feats);

    			//check to see if it's time to try to upload
    			samplecounter++;

    			if (samplecounter > AUDIO_UPLOAD_PERIOD_IN_TICKS) {
    				SetUpUpload();
    				samplecounter = 0;
    			}
    		}

    		break;
    	}

    	default: {
    		LOGI("AudioProcessingTask_Thread -- unrecognized message type\r\n");
    		break;
    	}
    	}


    	//end critical section
    	xSemaphoreGive(_mutex);


    	if (m.onFinished) {
    		m.onFinished(m.context);
    	}

	}
}
