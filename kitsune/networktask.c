#include "networktask.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "wifi_cmd.h"
#include <pb_encode.h>
#include "uartstdio.h"

#include "kit_assert.h"

#define NETWORK_TASK_QUEUE_DEPTH (10)
#define INITIAL_RETRY_PERIOD_COUNTS (1024)

static xSemaphoreHandle _network_mutex = NULL;

#define USE_DEBUG_PRINTF

#ifdef USE_DEBUG_PRINTF
#define DEBUG_PRINTF(...)  LOGI(__VA_ARGS__); LOGI("\r\n")
#else
static void nop(const char * foo,...) {  }
#define DEBUG_PRINTF(...) nop(__VA_ARGS__)
#endif

static NetworkResponse_t nettask_send(NetworkTaskServerSendMessage_t * message) {
	NetworkResponse_t response;
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

	retry_period = INITIAL_RETRY_PERIOD_COUNTS;
	attempt_count = 0;
	timeout_counts = message->retry_timeout;

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
				message->structdata) == 0) {
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
				vTaskDelay(retry_period);
				timeout_counts -= retry_period;
				retry_period <<= 1;
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
		message->response_callback(&response, decode_buf, decode_buf_size,message->context);
	}

	vPortFree(decode_buf);

	networktask_exit_critical_region();

	return response;
}
static int nettask_future_cb(void * out_buf, size_t out_size, void * ctx) {
	NetworkTaskServerSendMessage_t * message = (NetworkTaskServerSendMessage_t *)ctx;
	if (message->begin) {
		message->begin(&message);
	}

	if( message->response_handle ) {
		*message->response_handle = nettask_send(message);
	} else {
		nettask_send(message);
	}

	if( message->end ) {
		message->end(&message);
	}

	vPortFree(message);

	return out_size;
}
#include "hlo_async.h"

bool NetworkTask_SendProtobuf(bool blocking, const char * host,
		const char * endpoint, const pb_field_t fields[],
		const void * structdata, int32_t retry_time_in_counts,
		NetworkResponseCallback_t func, void * context) {
	NetworkTaskServerSendMessage_t * message;
	NetworkResponse_t response;
	hlo_future_t * nettask_future;

	message = pvPortMalloc( sizeof(NetworkTaskServerSendMessage_t));
	assert(message);
	memset(&message,0,sizeof(message));

	//craft message
	message->host = host;
	message->endpoint = endpoint;
	message->response_callback = func;
	message->retry_timeout = retry_time_in_counts;
	message->context = context;

	message->fields = fields;
	message->structdata = structdata;

	if( blocking ) {
		message->response_handle = &response;
	}
	DEBUG_PRINTF("NT %s",endpoint);

	nettask_future = hlo_future_create_task_bg( nettask_future_cb, (void*)message, 4096 );

	if( blocking ) {
		hlo_future_read(
						nettask_future,
						NULL,
						0);
	}
	hlo_future_destroy(nettask_future);

	return blocking ? response.success : false;
}


int NetworkTask_AddMessageToQueue(const NetworkTaskServerSendMessage_t * message) {
    hlo_future_create_task_bg(nettask_future_cb, (void*)message, 4096);
    return pdPASS;
}


int networktask_enter_critical_region()
{
	//LOGI("NT::ENTER CRITICAL REGION\n");
	return xSemaphoreTake(_network_mutex, portMAX_DELAY);
}

int networktask_exit_critical_region()
{
	//LOGI("NT::EXIT CRITICAL REGION\n");
	return xSemaphoreGive(_network_mutex);
}

void networktask_init(uint16_t stack_size)
{
	if(!_network_mutex)
	{
		_network_mutex = xSemaphoreCreateMutex();
	}

}


