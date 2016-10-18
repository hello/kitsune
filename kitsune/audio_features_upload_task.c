#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include <stdbool.h>
#include "audio_features_upload_task.h"
#include "protobuf/simple_matrix.pb.h"
#include "proto_utils.h"
#include "networktask.h"

typedef struct {
	const char * endpoint;
} AudioFeaturesUploadTaskConfig_t;

static xQueueHandle _uploadqueue = NULL;

const static AudioFeaturesUploadTaskConfig_t _config = {"/foobars"};

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

static void run(void * ctx) {
	const AudioFeaturesUploadTaskConfig_t * config = ctx;

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
					config->endpoint,
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

void audio_features_upload_task_create(void) {
	xTaskCreate(run,"audio features upload",1024/4,(void *)&_config,2,NULL);
}
