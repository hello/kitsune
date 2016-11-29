#include "FreeRTOS.h"
#include "audio_features_upload_task.h"
#include "audio_features_upload_task_helpers.h"

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
#define CIRCULAR_BUFFER_SIZE_BYTES (8192)
#define MAX_NUM_TICKS_TO_RESET (1 << 30)


/**************************
 *    STATIC VARIABLES
 */
static hlo_stream_t * _circstream = NULL;
static volatile int _is_waiting_for_uploading = 0;
static char _id_buf[128];
static RateLimiter_t _ratelimiterdata = {MAX_UPLOADS_PER_PERIOD,TICKS_PER_UPLOAD,0,0,0};
static SimpleMatrix _mat;


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

static void net_response(const NetworkResponse_t * response, char * reply_buf, int reply_sz,void * context) {
	hlo_stream_t * stream = (hlo_stream_t *)context;

	LOGI("audio_features_upload -- closing audio features stream\r\n");

	//close stream, no matter if anything was transfered
	hlo_stream_close(stream);

	//set stream ready for being re-opened
	_circstream = NULL;

	//set that we are ready to upload again
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

	mat->payload.funcs.encode = encode_repeated_streaming_bytes_and_mark_done;
	mat->payload.arg = bytestream;


}

//triggers upload, called from same thread as "audio_features_upload_task_buffer_bytes"
void audio_features_upload_trigger_async_upload(const char * net_id,const char * keyword,const uint32_t num_cols,FeaturesPayloadType_t feats_type) {
	NetworkTaskServerSendMessage_t netmessage;

	if (is_rate_limited(&_ratelimiterdata,xTaskGetTickCount())) {
		return;
	}

	if (_is_waiting_for_uploading) {
		return;
	}

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

	if (NetworkTask_AddMessageToQueue(&netmessage) == pdFALSE) {
		LOGE("audio_features_upload -- UNABLE TO ADD TO NETWORK QUEUE\r\n");
		return;
	}

	LOGI("audio_features_upload -- added to network queue\r\n");

	//toggle state if adding message to queue was successful
	_is_waiting_for_uploading = 1;


}









