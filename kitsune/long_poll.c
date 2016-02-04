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
#include "sleep_sounds.pb.h"

#define LONG_POLL_HOST "long-poll.sense.in"
#define LONG_POLL_ENDPOINT "/poll"

#define _MAX_RX_RECEIPTS 10

xQueueHandle _rx_queue = 0;

static void _on_sleep_sounds( SleepSoundsCommand * cmd ) {}


static bool _on_message(pb_istream_t *stream, const pb_field_t *field, void **arg) {
	Message message;

	LOGI("message parsing\n" );
	if( !pb_decode(stream,Message_fields,&message) ) {
		LOGI("message parse fail \n" );
		return false;
	}

	if( message.has_message_id ) {
		xQueueSend(_rx_queue, (void* )&message.message_id, 0);
	}
	if( message.has_sleep_sounds_command ) {
		_on_sleep_sounds( &message.sleep_sounds_command );
	}

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
				( success =  pb_encode_tag_for_field(stream, field) && pb_encode_fixed64(stream, &id ) ) ) {}
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
		while (!wifi_status_get(HAS_IP)) {
			vTaskDelay(1000);
		}
		if( send_data_pb(LONG_POLL_HOST,
				LONG_POLL_ENDPOINT,
				&decode_buf,
				&decode_buf_size,
				ReceiveMessageRequest_fields,
				&request,
				&pb_cb, &sock, SOCKET_SEC_NONE ) != 0 ) {
			if( retries++ < 5 ) {
				vTaskDelay( (1<<retries)*1000 );
			} else {
				vTaskDelay( 32000 );
			}
		} else {
			retries = 0;
		}
	}
}

void long_poll_task_init(uint16_t stack_size)
{
	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(long_poll_task, "long_poll", stack_size, NULL, 4, NULL);
}


