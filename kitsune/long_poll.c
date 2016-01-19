#include "networktask.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "wifi_cmd.h"
#include <pb_encode.h>
#include "uartstdio.h"

#include "kit_assert.h"
#include "sl_sync_include_after_simplelink_header.h"

#define LONG_POLL_HOST "long-poll.sense.in"
#define LONG_POLL_ENDPOINT "/poll"

static void _get_long_poll_response(pb_field_t ** fields, void ** structdata){
	*fields = (pb_field_t *)SyncResponse_fields;
	*structdata = pvPortMalloc(sizeof(SyncResponse));
	assert(*structdata);
	if( *structdata ) {
		SyncResponse * response_protobuf = *structdata;
		memset(response_protobuf, 0, sizeof(SyncResponse));
		response_protobuf->pill_settings.funcs.decode = on_pill_settings;
		response_protobuf->files.funcs.decode = _on_file_download;
	}
}
static void _free_long_poll_response(void * structdata){
	vPortFree( structdata );
}

static void _on_long_poll_response_success( void * structdata){
	LOGF("_on_long_poll_response_success\r\n");
	_on_response_protobuf((SyncResponse*)structdata);
}
static void _on_long_poll_response_failure( ){
    LOGF("_on_long_poll_response_failure\r\n");
}

static void long_poll_task(void * networkdata) {
	int sock = -1;
	unsigned int last_rx = 0;
	batched_periodic_data data_batched = {0}; //todo swap with long poll specific stuff

	for (; ;) {
		char * decode_buf = pvPortMalloc(SERVER_REPLY_BUFSZ);
	    assert(decode_buf);
	    memset(decode_buf, 0, SERVER_REPLY_BUFSZ);
	    size_t decode_buf_size = SERVER_REPLY_BUFSZ;

	    protobuf_reply_callbacks pb_cb;

	    pb_cb.get_reply_pb = _get_long_poll_response;
	    pb_cb.free_reply_pb = _free_long_poll_response;
	    pb_cb.on_pb_success = _on_long_poll_response_success;
	    pb_cb.on_pb_failure = _on_long_poll_response_failure;

		while (1) {
			DEBUG_PRINTF("N %d",last_rx);

			send_data_pb(LONG_POLL_HOST,
					LONG_POLL_ENDPOINT,
					&decode_buf,
					&decode_buf_size,
					SyncResponse_fields,
					batched_periodic_data,
					&pb_cb, &sock );
		}
	}
}

void long_poll_task_init(uint16_t stack_size)
{
	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(long_poll_task, "long_poll", stack_size, NULL, 2, NULL);
}


