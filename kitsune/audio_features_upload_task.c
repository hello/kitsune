#include "FreeRTOS.h"
#include "audio_features_upload_task.h"
#include "audio_features_upload_task_helpers.h"

#include "queue.h"
#include "task.h"
#include "semphr.h"

#include <stdbool.h>
#include "protobuf/simple_matrix.pb.h"
#include "proto_utils.h"
#include "networktask.h"
#include "endpoints.h"
#include "hlo_circbuf_stream.h"
#include "tensor/features_types.h"


/**************************
 *    STRUCT DEFS
 */

typedef struct {
	hlo_stream_t * stream;
	const char * keyword;
	const char * net_id;
	int num_cols;
	FeaturesPayloadType_t feats_type;
} AudioFeaturesUploadTaskMessage_t;

/**************************
 *    CONSTANTS
 */

//once per five minutes
#define TICKS_PER_UPLOAD (300000)
#define MAX_UPLOADS_PER_PERIOD (2)
#define CIRCULAR_BUFFER_SIZE_BYTES (8192)
#define MAX_NUM_TICKS_TO_RESET (1 << 30)


/**************************
 *    STATIC VARIABLES
 */
static xQueueHandle _uploadqueue = NULL; //initialized on creation of task
static hlo_stream_t * _circstream = NULL;
static volatile int _is_waiting_for_uploading = 0;
static char _id_buf[128];
static RateLimiter_t _ratelimiterdata = {MAX_UPLOADS_PER_PERIOD,TICKS_PER_UPLOAD,0,0,0};


//meant to be called from the same thread that triggers the upload
void audio_features_upload_task_buffer_bytes(void * data, uint32_t len) {
	if (_is_waiting_for_uploading) {
		return;
	}

	if (!_circstream) {
		_circstream = hlo_circbuf_stream_open(CIRCULAR_BUFFER_SIZE_BYTES);
	}

	hlo_stream_write(_circstream,data,len);

}


//triggers upload, called from same thread as "audio_features_upload_task_buffer_bytes"
void audio_features_upload_trigger_async_upload(const char * net_id,const char * keyword,const uint32_t num_cols,FeaturesPayloadType_t feats_type) {
	AudioFeaturesUploadTaskMessage_t message;

	if (is_rate_limited(xTaskGetTickCount(),&_ratelimiterdata)) {
		return;
	}

	if (_is_waiting_for_uploading) {
		return;
	}

	memset(&message,0,sizeof(message));

	message.net_id = net_id;
	message.keyword = keyword;
	message.num_cols = num_cols;
	message.feats_type = feats_type;
	message.stream = _circstream;

	if (xQueueSend(_uploadqueue,&message,0) == pdFALSE) {
		//log that queue was full
		LOGI("audio_features_upload_task queue full\r\n");
		return;
	}

	//otherwise if adding to queue succeeded, block further buffering and prepare for next time
	//the stream will close itself (and thus free memory) when it's done
	_circstream = NULL;
	_is_waiting_for_uploading = 1;
}




static void net_response(const NetworkResponse_t * response, char * reply_buf, int reply_sz,void * context) {
	_is_waiting_for_uploading = 0;
}


static SimpleMatrixDataType map_type(FeaturesPayloadType_t feats_type) {
	switch (feats_type) {

	case feats_sint8:
		return SimpleMatrixDataType_SINT8;

	default:
		return SimpleMatrixDataType_SINT8;
	}
}




static void setup_protbuf(SimpleMatrix * mat,hlo_stream_t * bytestream, const char * net_id, const char * keyword,const int num_cols,FeaturesPayloadType_t feats_type) {
	memset(mat,0,sizeof(SimpleMatrix));


	strncpy(_id_buf,net_id,sizeof(_id_buf));
	strncat(_id_buf,"+",sizeof(_id_buf));
	strncat(_id_buf,keyword,sizeof(_id_buf));

	mat->id.arg = (void *) _id_buf;
	mat->id.funcs.encode = _encode_string_fields;

	mat->has_data_type = true;
	mat->data_type = map_type(feats_type);

	mat->has_num_cols = true;
	mat->num_cols = num_cols;

	mat->payload.funcs.encode = encode_repeated_streaming_bytes;
	mat->payload.arg = bytestream;


}

void audio_features_upload_task(void * ctx) {

	_uploadqueue = xQueueCreate(1,sizeof(AudioFeaturesUploadTaskMessage_t));

	//run indefinitely
	while (1) {
		AudioFeaturesUploadTaskMessage_t message;

		//block until something is added to the queue
		if (xQueueReceive(_uploadqueue,(void *)&message,portMAX_DELAY)) {

			//set up protobuf encode for streaming
			SimpleMatrix mat;

			setup_protbuf(
					&mat,
					message.stream,
					message.net_id,
					message.keyword,
					message.num_cols,
					message.feats_type);


			if (!NetworkTask_SendProtobuf(
					true,
					DATA_SERVER,
					AUDIO_FEATURES_ENDPOINT,
					SimpleMatrix_fields,
					&mat,
					0,
					net_response,
					NULL,
					NULL,
					false)) {

				//log failure
			}
		}
	}
}
