#include "networktask.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "wifi_cmd.h"
#include <pb_encode.h>
#include "uartstdio.h"

#include "kit_assert.h"
#include "proto_utils.h"
#include "sl_sync_include_after_simplelink_header.h"

#include "messeji.pb.h"

#include "audiotask.h"
#include "audio_commands.pb.h"

#include "hellofilesystem.h"

#include "sys_time.h"

#define _MAX_RX_RECEIPTS 10

#include "endpoints.h"
extern volatile bool use_dev_server;
char * get_messeji_server(void){
	if(use_dev_server){
		return DEV_MESSEJI_SERVER;
	}
	return PROD_MESSEJI_SERVER;
}

#define LONG_POLL_HOST MESSEJI_SERVER
#define LONG_POLL_ENDPOINT MESSEJI_ENDPOINT

xQueueHandle _rx_queue = 0;

static void _on_play_audio( PlayAudio * cmd ) {
	AudioPlaybackDesc_t desc={0};

	if( cmd->has_duration_seconds ) {
		desc.durationInSeconds = cmd->duration_seconds;
	} else {
		desc.durationInSeconds = 0;
	}
	desc.stream = fs_stream_open_media(cmd->file_path,INT32_MAX);
	ustrncpy(desc.source_name, cmd->file_path, sizeof(desc.source_name));
	desc.volume = cmd->volume_percent * 60 / 100; //convert from percent to codec range
	desc.fade_in_ms = cmd->fade_in_duration_seconds * 1000;
	desc.fade_out_ms = cmd->fade_out_duration_seconds * 1000;
	if( cmd->has_timeout_fade_out_duration_seconds ) {
		desc.to_fade_out_ms = cmd->timeout_fade_out_duration_seconds * 1000;
	} else {
		desc.to_fade_out_ms = desc.fade_out_ms;
	}

	desc.onFinished = NULL;
	desc.rate = AUDIO_SAMPLE_RATE;
	desc.context = NULL;
	desc.p = NULL;

	AudioTask_StartPlayback(&desc);

}
static void _on_stop_audio( StopAudio * cmd ) {
	//TODO use cmd->fade_out_duration_seconds; ?
	AudioTask_StopPlayback();
}

bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg);

static bool _on_message(pb_istream_t *stream, const pb_field_t *field, void **arg) {
	Message message;

	LOGI("message parsing\n");
	message.sender_id.funcs.encode = NULL;
	message.sender_id.arg = NULL;
	message.sender_id.funcs.decode = _decode_string_field;

	if (!pb_decode(stream, Message_fields, &message)) {
		LOGI("message parse fail \n");
		return false;
	}

	if (message.has_message_id) {
		LOGI("message id %d\n", message.message_id );
		xQueueSend(_rx_queue, (void* )&message.message_id, 0);
	}
	if (message.has_play_audio) {
		_on_play_audio(&message.play_audio);
	}
	if (message.has_stop_audio) {
		_on_stop_audio(&message.stop_audio);
	}

	vPortFree(message.sender_id.arg);
	return true;
}
static void _get_long_poll_response(pb_field_t ** fields, void ** structdata){
	*fields = (pb_field_t *)BatchMessage_fields;
	*structdata = pvPortMalloc(sizeof(BatchMessage));
	assert(*structdata);
	if( *structdata ) {
		BatchMessage * response_protobuf = *structdata;
		memset(response_protobuf, 0, sizeof(BatchMessage));
		response_protobuf->message.funcs.decode = _on_message;
	}
}
static void _free_long_poll_response(void * structdata){
	vPortFree( structdata );
}

static void _on_long_poll_response_success( void * structdata){
	LOGF("_on_long_poll_response_success\r\n");
}
static void _on_long_poll_response_failure( ){
    LOGF("_on_long_poll_response_failure\r\n");
}

bool _encode_read_id(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	int64_t id;
	bool success = TRUE;
	if( _rx_queue ) {
		while( xQueueReceive(_rx_queue, &id, 0 ) &&
				//todo check if tag_for_field is necessary here
				( success =  pb_encode_tag_for_field(stream, field) && pb_encode_varint(stream, id ) ) ) {
			LOGI("ack message id %d\n", id );
		}
	}
	return success;
}


static void long_poll_task(void * networkdata) {
	int sock = -1;
	int retries = 0;
	ReceiveMessageRequest request;
    protobuf_reply_callbacks pb_cb;

	char * decode_buf = pvPortMalloc(SERVER_REPLY_BUFSZ);
    assert(decode_buf);
    memset(decode_buf, 0, SERVER_REPLY_BUFSZ);
    size_t decode_buf_size = SERVER_REPLY_BUFSZ;

    _rx_queue = xQueueCreate(_MAX_RX_RECEIPTS, sizeof(int64_t));

    pb_cb.get_reply_pb = _get_long_poll_response;
    pb_cb.free_reply_pb = _free_long_poll_response;
    pb_cb.on_pb_success = _on_long_poll_response_success;
    pb_cb.on_pb_failure = _on_long_poll_response_failure;

    request.sense_id.funcs.encode = encode_device_id_string;
    request.message_read_id.funcs.encode = _encode_read_id;

	for (; ;) {
		wait_for_time(WAIT_FOREVER); //need time for SSL
		if( send_data_pb(LONG_POLL_HOST,
				LONG_POLL_ENDPOINT,
				&decode_buf,
				&decode_buf_size,
				ReceiveMessageRequest_fields,
				&request,
				&pb_cb, &sock, SOCKET_SEC_SSL ) != 0 ) {
			if( retries++ < 5 ) {
				vTaskDelay( (1<<retries)*1000 );
			} else {
				vTaskDelay( 32000 );
			}
		} else {
			retries = 0;
		}
	}
	//shouldn't get here, but for consistency...
	//vPortFree(decode_buf);
}

void long_poll_task_init(uint16_t stack_size)
{
	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(long_poll_task, "long_poll", stack_size, NULL, 2, NULL);
}


