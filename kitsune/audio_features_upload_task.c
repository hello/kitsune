#include "FreeRTOS.h"
#include "audio_features_upload_task.h"

#include "queue.h"
#include "task.h"
#include "semphr.h"

#include <stdbool.h>
#include "protobuf/simple_matrix.pb.h"
#include "proto_utils.h"
#include "networktask.h"
#include "endpoints.h"
#include "hlo_circbuf_stream.h"

static xQueueHandle _uploadqueue = NULL;

static hlo_stream_t * _circstream = NULL;
static volatile int _is_waiting_for_uploading = 0;

#define CIRCULAR_BUFFER_SIZE_BYTES (8192)

//meant to be called from the same thread that triggers the upload
void audio_features_upload_task_buffer_bytes(void * data, uint32_t len) {
	if (_is_waiting_for_uploading) {
		return;
	}

	if (!_circstream) {
		_circstream = hlo_circbuf_stream_open(CIRCULAR_BUFFER_SIZE_BYTES);
	}

	_circstream->impl.write(_circstream->ctx,data,len);

}

void audio_features_upload_trigger_async_upload(const char * id,const uint32_t num_cols,FeaturesPayloadType_t feats_type) {
	AudioFeaturesUploadTaskMessage_t message;

	if (_is_waiting_for_uploading) {
		return;
	}

	_is_waiting_for_uploading = 1;

	memset(&message,0,sizeof(message));

	message.id = id;
	message.num_cols = num_cols;
	message.feats_type = feats_type;
	message.stream = _circstream;

	audio_features_upload_task_add_message(&message);
}


static bool encode_repeated_streaming_bytes(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    unsigned char buffer[256];
	hlo_stream_t * hlo_stream = *arg;
    int bytes_read = 0;

    if(!hlo_stream) {
        return false;
    }

    while (1) {
    	bytes_read = hlo_stream->impl.read(hlo_stream->ctx,buffer,sizeof(buffer));

    	if (bytes_read <= 0) {
    		break;
    	}

        //write string tag for delimited field
        if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
            return false;
        }

        //write size
    	if (!pb_encode_varint(stream, (uint64_t)bytes_read)) {
    	    return false;
    	}

    	//write buffer
    	if (!pb_write(stream, buffer, bytes_read)) {
    		return false;
    	}
    }

	//close stream, always
	hlo_stream->impl.close(hlo_stream->ctx);

	//reset
	_is_waiting_for_uploading = 0;
	_circstream = NULL;

    return true;
}

static void net_response(const NetworkResponse_t * response, char * reply_buf, int reply_sz,void * context) {

}

void audio_features_upload_task_add_message(const AudioFeaturesUploadTaskMessage_t * message) {
	if (!xQueueSend(_uploadqueue,message,0)) {
		//log that queue was full
	}
}

static SimpleMatrixDataType map_type(FeaturesPayloadType_t feats_type) {
	switch (feats_type) {

	case feats_sint8:
		return SimpleMatrixDataType_SINT8;

	default:
		return SimpleMatrixDataType_SINT8;
	}
}




static void setup_protbuf(SimpleMatrix * mat,hlo_stream_t * bytestream, const char * id,const int num_cols,FeaturesPayloadType_t feats_type) {
	memset(mat,0,sizeof(SimpleMatrix));
	mat->id.arg = (void *) id;
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
					message.id,
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
