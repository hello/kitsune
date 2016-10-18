#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include <stdbool.h>
#include "audio_features_upload_task.h"

#include "networktask.h"

static xQueueHandle _uploadqueue = NULL;

typedef struct {
	const char * endpoint;
} AudioFeaturesUploadTaskConfig_t;

typedef struct {


} AudioFeaturesUploadTaskMessage_t;


static void net_response(const NetworkResponse_t * response, char * reply_buf, int reply_sz,void * context) {

}

void audio_features_upload_task_add_message(const AudioFeaturesUploadTaskMessage_t * message) {
	xQueueSend(_uploadqueue,message,0);
}

static void run(void * ctx) {
	const AudioFeaturesUploadTaskConfig_t * config = ctx;

	_uploadqueue = xQueueCreate(1,sizeof(AudioFeaturesUploadTaskMessage_t));


	while (1) {
		//run indefinitely
		AudioFeaturesUploadTaskMessage_t message;

		if (xQueueReceive(_uploadqueue,(void *)&message,portMAX_DELAY)) {

			//get m

			if (!NetworkTask_SendProtobuf(true,DATA_SERVER,config->endpoint,
					const pb_field_t fields[],void * structdata,
					0,net_response, NULL,
					NULL, false)) {

				//log failure
			}
		}
	}
}
