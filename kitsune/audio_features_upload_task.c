#include "FreeRTOS.h"
#include "audio_features_upload_task.h"
#include "audio_features_upload_task_helpers.h"
#include <queue.h>

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



/**************************
 *    CONSTANTS
 */

//once per five minutes
#define TICKS_PER_UPLOAD (300000)
#define MAX_UPLOADS_PER_PERIOD (2)
#define CIRCULAR_BUFFER_SIZE_BYTES (16000)
#define MAX_NUM_TICKS_TO_RESET (1 << 30)

#define QUEUE_LENGTH (1)
#define DELAY_TIME (10000)

/**************************
 *    STATIC VARIABLES
 */
static hlo_stream_t * _circstream = NULL;
static volatile int _is_waiting_for_uploading = 0;
static char _id_buf[128];
static RateLimiter_t _ratelimiterdata = {MAX_UPLOADS_PER_PERIOD,TICKS_PER_UPLOAD,0,0,0};
static SimpleMatrix _mat;
static volatile bool _disabled_uploads = true;

//delayed send queue
static xQueueHandle _delayed_send_queue = NULL;
static xQueueHandle _wakeup_queue = NULL;

//meant to be called from the same thread that triggers the upload
void audio_features_upload_task_buffer_bytes(void * data, uint32_t len) {
	if (_is_waiting_for_uploading) {
		return;
	}

	if (!_circstream) {
		LOGI("audio_features_upload -- creating audio features circular buffer stream of size %d bytes\r\n",CIRCULAR_BUFFER_SIZE_BYTES);
		_circstream = hlo_circbuf_stream_open(CIRCULAR_BUFFER_SIZE_BYTES);
	}

	hlo_stream_write(_circstream,data,len);

}

void audio_features_upload_set_upload_status(bool enabled) {
	_disabled_uploads = !enabled;
}

static void cleanup(hlo_stream_t * stream) {
	//close stream, no matter if anything was transfered
	if (stream) {

		LOGI("audio_features_upload -- closing audio features stream\r\n");

		hlo_stream_close(stream);
	}

	//set stream ready for being re-opened
	_circstream = NULL;

	//set that we are ready to upload again
	_is_waiting_for_uploading = 0;

	LOGI("audio_features_upload -- reset state\r\n");

}

static void net_response(const NetworkResponse_t * response, char * reply_buf, int reply_sz,void * context) {
	hlo_stream_t * stream = (hlo_stream_t *)context;

	cleanup(stream);

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

	mat->payload.funcs.encode = encode_repeated_streaming_bytes_and_mark_done;
	mat->payload.arg = bytestream;

	mat->device_id.funcs.encode = encode_device_id_string;

}
#define DBG_UPLOADER(...)

void audio_features_upload_task(void * not_used) {
	int ctr;

	_delayed_send_queue = xQueueCreate(QUEUE_LENGTH,sizeof(NetworkTaskServerSendMessage_t));
	_wakeup_queue =  xQueueCreate(2,sizeof(int));

	NetworkTaskServerSendMessage_t message;

	for (; ;) {

		//only proceed if we get the last wakeup and no more wakups come during the delay time.
		if( xQueueReceive( _wakeup_queue, &ctr, portMAX_DELAY ) ) {
			DBG_UPLOADER("\n\t\t\t\t\t got something in queue! %d %d\n", uxQueueMessagesWaiting( _wakeup_queue ), ctr );
			vTaskDelay( DELAY_TIME );
			if( uxQueueMessagesWaiting( _wakeup_queue ) ) {
				DBG_UPLOADER("\n\t\t\t\t\t messages in queue! %d\n", uxQueueMessagesWaiting( _wakeup_queue ) );
				continue;
			}
		}
		DBG_UPLOADER("\n\t\t\t\t\t uploadingsssss %d %d\n", uxQueueMessagesWaiting( _wakeup_queue ), ctr );

		//if no delay, and there is a message
		if ( xQueueReceive( _delayed_send_queue, &message, 0 ) ) {
			LOGI("audio_features_upload -- sending\r\n");

			//relay
			if (NetworkTask_AddMessageToQueue(&message) == pdFALSE) {
				LOGE("audio_features_upload -- UNABLE TO ADD TO NETWORK QUEUE\r\n");

				cleanup((hlo_stream_t *)  message.context);
			}
		}
	}
}


//triggers upload, called from same thread as "audio_features_upload_task_buffer_bytes"
void audio_features_upload_trigger_async_upload(const char * net_id,const char * keyword,const uint32_t num_cols,FeaturesPayloadType_t feats_type) {
	static int ctr = 0;
	NetworkTaskServerSendMessage_t netmessage;

	if (_disabled_uploads) {
		LOGI("audio_features_upload -- uploads are disabled, ignoring upload request\r\n");
		return;
	}


	//stop collecting data immediately
	_is_waiting_for_uploading = 1;

	memset(&netmessage,0,sizeof(netmessage));
	memset(&_mat,0,sizeof(_mat));

	//set up protobuf encode for streaming
	setup_protbuf(&_mat,_circstream,net_id,keyword,num_cols,feats_type);

	netmessage.structdata = &_mat;
	netmessage.fields = SimpleMatrix_fields;
	netmessage.endpoint = AUDIO_KEYWORD_FEATURES_ENDPOINT;
	netmessage.host = DATA_SERVER;
	netmessage.response_callback = net_response;
	netmessage.context = _circstream;

	//if it fails it's because the queue is overflowing, and that's ok
	 ++ctr;
	xQueueSend(_wakeup_queue,&ctr,0);
	DBG_UPLOADER("\n\t\t\t\t\t queuing %d %d\n", uxQueueMessagesWaiting( _wakeup_queue ), ctr );

	if( uxQueueMessagesWaiting( _wakeup_queue ) > 1 ) {
		LOGE("audio_features_upload -- too many waiting\r\n");
		return;
	}

	if (_is_waiting_for_uploading) {
		LOGI("audio_features_upload -- upload already in the pipe, ignoring upload request\r\n");
		return;
	}
	if (xQueueSend(_delayed_send_queue,&netmessage,0) == pdFALSE) {
		LOGE("audio_features_upload -- UNABLE TO ADD TO QUEUE\r\n");
		return;
	}
	if (is_rate_limited(&_ratelimiterdata,xTaskGetTickCount())) {
		LOGI("audio_features_upload -- rate limited, ignoring upload request\r\n");
		return;
	}

	LOGI("audio_features_upload -- added to delay queue\r\n");

}









