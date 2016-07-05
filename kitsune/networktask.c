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

#define NETWORK_TASK_QUEUE_DEPTH (10)
#define INITIAL_RETRY_PERIOD_COUNTS (1024)

#if 0
static xQueueHandle _asyncqueue = NULL;
static xSemaphoreHandle _network_mutex = NULL;
#endif

#define USE_DEBUG_PRINTF

#ifdef USE_DEBUG_PRINTF
#define DEBUG_PRINTF(...)  LOGI(__VA_ARGS__); LOGI("\r\n")
#else
static void nop(const char * foo,...) {  }
#define DEBUG_PRINTF(...) nop(__VA_ARGS__)
#endif

static void Init(NetworkTaskData_t * info) {

#if 0
	if (!_asyncqueue) {
		_asyncqueue = xQueueCreate(NETWORK_TASK_QUEUE_DEPTH,sizeof(NetworkTaskServerSendMessage_t));
	}

	if(!_network_mutex)
	{
		_network_mutex = xSemaphoreCreateMutex();
	}
#endif
}

void unblock_sync(void * data) {
#if 0
	NetworkTaskServerSendMessage_t * msg = (NetworkTaskServerSendMessage_t*)data;
	assert( msg->sync );
	xSemaphoreGive(msg->sync); //todo maybe here
#endif
}

bool NetworkTask_SendProtobuf(bool blocking, char * host,
		const char * endpoint, const pb_field_t fields[],
		void * structdata, int32_t retry_time_in_counts,
		NetworkResponseCallback_t func, void * context,
		protobuf_reply_callbacks *pb_cb,
		bool is_high_priority) {
#if 0
	NetworkTaskServerSendMessage_t message;
	NetworkResponse_t response;
	BaseType_t ret = pdFALSE;
	memset(&message,0,sizeof(message));

	//craft message
	message.host = host;
	message.endpoint = endpoint;
	message.response_callback = func;
	message.retry_timeout = retry_time_in_counts;
	message.context = context;
	message.priority = is_high_priority;

	message.fields = fields;
	message.structdata = structdata;

	message.has_pb_cb = false;
	if( pb_cb ) {
		message.pb_cb = *pb_cb;
		message.has_pb_cb = true;
	}

	assert( _asyncqueue );

	if( blocking ) {
		message.sync = xSemaphoreCreateBinary();
		assert( message.sync );
		message.end = unblock_sync;
		message.response_handle = &response;
	}
	DEBUG_PRINTF("NT %s",endpoint);

	assert( _asyncqueue );

	if(is_high_priority){
		DISP("NT: queued front\r\n");
		ret = xQueueSendToFront(_asyncqueue, (const void *)&message, portMAX_DELAY);
	}else{
		DISP("NT: queued back\r\n");
		ret = xQueueSend(_asyncqueue, (const void *)&message, portMAX_DELAY);
	}
	//add to queue
	if(ret != pdTRUE)
	{
		LOGE("Cannot send to _asyncqueue\n");

		if( message.sync ) {
			vSemaphoreDelete(message.sync);
		}
		return false;
	}

	if( message.sync ) {
	    xSemaphoreTake(message.sync, portMAX_DELAY);
	    vSemaphoreDelete(message.sync);
		DEBUG_PRINTF("NT bl ret\n");
		return response.success;
	}
	DEBUG_PRINTF("NT ret\n");
#endif
	return false;
}

static NetworkResponse_t nettask_send(NetworkTaskServerSendMessage_t * message, int * sock) {

	NetworkResponse_t response;
#if 0
	int32_t timeout_counts;
	int32_t retry_period;
	uint32_t attempt_count;

	memset(&response,0,sizeof(response));

	if(networktask_enter_critical_region() != pdTRUE)
	{
		LOGE("Cannot networktask_enter_critical_region\n");
		return response;
	}
	char * decode_buf = pvPortMalloc(SERVER_REPLY_BUFSZ);
    assert(decode_buf);
    memset(decode_buf, 0, SERVER_REPLY_BUFSZ);
    size_t decode_buf_size = SERVER_REPLY_BUFSZ;

	retry_period = INITIAL_RETRY_PERIOD_COUNTS + (rand()%2048); //add some jitter so a mass extinction event doesn't ddos
	attempt_count = 0;
	timeout_counts = message->retry_timeout;

	message->interrupted = false;

	while (1) {

		DEBUG_PRINTF("NT %s%s -- %d",message->host,message->endpoint,attempt_count);

		/* prepare to prepare */
		if (message->prepare) {
			message->prepare(message->prepdata);
		}

		//push to server
		if (send_data_pb(message->host,
				message->endpoint,
				&decode_buf,
				&decode_buf_size,
				message->fields,
				message->structdata,
				message->has_pb_cb ? &message->pb_cb : NULL, sock, SOCKET_SEC_SSL ) == 0) {
			response.success = true;
		} else {
			//failed to push, now what?
			response.success = false;
			response.flags |= NETWORK_RESPONSE_FLAG_NO_CONNECTION;
		}

		/* unprepare */
		if (message->unprepare) {
			message->unprepare(message->prepdata);
		}


		//if failed response and there's some timeout time left, go delay and try again.
		if (!response.success && timeout_counts > 0) {

			if (timeout_counts < retry_period) {
				DEBUG_PRINTF("NT waiting %d ticks",retry_period);
				vTaskDelay(retry_period);
				timeout_counts = 0;
			}
			else {
				DEBUG_PRINTF("NT waiting %d ticks",retry_period);
				int32_t countdown = retry_period;
				while( countdown > 0 ) {
					NetworkTaskServerSendMessage_t m;
					if( xQueuePeek( _asyncqueue, &m, 0 ) ) {
						if( m.priority ) {
							message->interrupted = true;
							if( xQueueSend( _asyncqueue, ( const void * ) message, 0 ) ) {
								DEBUG_PRINTF("NT switching to higher priority message");
								goto next_message;
							}
						}
					}
					countdown -= 500;
				}
				vTaskDelay(retry_period);
				timeout_counts -= retry_period;
				retry_period <<= 1;
				if( retry_period > 30000 ) {
					retry_period = 30000;
				}
			}
		}
		else {
			break;
		}

		attempt_count++;

	}

	if (response.success) {
		DEBUG_PRINTF("NT %s%s (success)",message->host,message->endpoint);
	}
	else {
		DEBUG_PRINTF("NT %s%s (failure)",message->host,message->endpoint);
	}

	//let the requester know we are done
	if (message->response_callback) {
		DEBUG_PRINTF("NT CB %x\n",message->response_callback);
		message->response_callback(&response, decode_buf, decode_buf_size,message->context);
	}

	next_message:
	vPortFree(decode_buf);

	networktask_exit_critical_region();

#endif
	return response;

}

static void NetworkTask_Thread(void * networkdata) {
#if 0
	NetworkTaskServerSendMessage_t message;
	int sock = -1;

	for (; ;) {
		xQueueReceive( _asyncqueue, &message, portMAX_DELAY );

		if( message.response_handle ) {
			*message.response_handle = nettask_send(&message, &sock);
		} else {
			nettask_send(&message, &sock);
		}

		if( !message.interrupted && message.end ) {
			message.end(&message);
		}
	}
#endif
}


int NetworkTask_AddMessageToQueue(const NetworkTaskServerSendMessage_t * message) {
#if 0

    return xQueueSend( _asyncqueue, ( const void * ) message, 10 );
#endif
	return 0;
}


int networktask_enter_critical_region()
{
#if 0

	LOGI("NT::ENTER\n");
	return xSemaphoreTake(_network_mutex, portMAX_DELAY);
#endif
	return 0;
}

int networktask_exit_critical_region()
{
#if 0
	LOGI("NT::EXIT\n");
	return xSemaphoreGive(_network_mutex);
#endif
	return 0;
}

void networktask_init(uint16_t stack_size)
{
#if 0
	// In this way the network task is encapsulated to its own module
	// no semaphore needs to expose to outside
	NetworkTaskData_t network_task_data;
	memset(&network_task_data, 0, sizeof(network_task_data));

	Init(&network_task_data);

	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(NetworkTask_Thread, "networkTask", stack_size, &network_task_data, 2, NULL);
#endif
}


