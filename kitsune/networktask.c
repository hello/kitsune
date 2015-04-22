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
#define NUM_RECEIVE_RETRIES (10)

static xQueueHandle _asyncqueue = NULL;
static xSemaphoreHandle _network_mutex = NULL;

#define USE_DEBUG_PRINTF

#ifdef USE_DEBUG_PRINTF
#define DEBUG_PRINTF(...)  LOGI(__VA_ARGS__); LOGI("\r\n")
#else
static void nop(const char * foo,...) {  }
#define DEBUG_PRINTF(...) nop(__VA_ARGS__)
#endif

static void Init(NetworkTaskData_t * info) {

	if (!_asyncqueue) {
		_asyncqueue = xQueueCreate(NETWORK_TASK_QUEUE_DEPTH,sizeof(NetworkTaskServerSendMessage_t));
	}

	if(!_network_mutex)
	{
		_network_mutex = xSemaphoreCreateMutex();
	}

}

static uint32_t EncodePb(pb_ostream_t * stream, void * data) {
	network_encode_data_t * encodedata = (network_encode_data_t *)data;
	uint32_t ret = false;

	if (encodedata && encodedata->encodedata) {
	ret = pb_encode(stream,encodedata->fields,encodedata->encodedata);
	}

//	DEBUG_PRINTF("NT::EncodePb -- got callback");

	return ret;
}
static void FreeMe(void * data) {
	if(data) {
		vPortFree(data);
	}
}

int NetworkTask_AsynchronousSendProtobuf(const char * host,
		const char * endpoint, const pb_field_t fields[],
		const void * structdata, int32_t retry_time_in_counts,
		NetworkResponseCallback_t func, void * data) {
	NetworkTaskServerSendMessage_t message;

	network_encode_data_t * encodedata = pvPortMalloc(sizeof(network_encode_data_t));
	assert(encodedata);
	memset(&message,0,sizeof(message));

	encodedata->fields = fields;
	encodedata->encodedata = structdata;

	//craft message
	message.host = host;
	message.endpoint = endpoint;
	message.response_callback = func;
	message.retry_timeout = retry_time_in_counts;
	message.context = data;

	message.encode = EncodePb;
	message.encodedata = encodedata;

	message.terminate = FreeMe;
	message.terminate_data = encodedata;

	assert( _asyncqueue );

	DEBUG_PRINTF("NT async %s",endpoint);

	//add to queue
	if(xQueueSend( _asyncqueue, ( const void * )&message, 1000 ) != pdTRUE)
	{
		LOGE("Cannot send request to _asyncqueue\n");
		return -1;
	}

	return 0;

}

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
	message->decode_buf = pvPortMalloc(SERVER_REPLY_BUFSZ);
    assert(message->decode_buf);
    memset(message->decode_buf, 0, SERVER_REPLY_BUFSZ);
    message->decode_buf_size = SERVER_REPLY_BUFSZ;

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
		if (send_data_pb_callback(message->host,
				message->endpoint,
				(char*)message->decode_buf,
				message->decode_buf_size,
				message->encodedata,
				message->encode,
				NUM_RECEIVE_RETRIES) == 0) {
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
		message->response_callback(&response, message->decode_buf, message->decode_buf_size,message->context);
	}

	vPortFree(message->decode_buf);

	if( message->terminate ) {
		message->terminate(message->terminate_data);
	}

	networktask_exit_critical_region();;

	return response;
}

void unblock_sync(void * data) {
	NetworkTaskServerSendMessage_t * msg = (NetworkTaskServerSendMessage_t*)data;
    xSemaphoreGive(msg->sync);
}

bool NetworkTask_SynchronousSendProtobuf(const char * host,
		const char * endpoint, const pb_field_t fields[],
		const void * structdata, int32_t retry_time_in_counts,
		NetworkResponseCallback_t func, void * context) {
	NetworkTaskServerSendMessage_t message;
	NetworkResponse_t response;
	network_encode_data_t encodedata = {0};

	memset(&message,0,sizeof(message));

	encodedata.fields = fields;
	encodedata.encodedata = structdata;

	//craft message
	message.host = host;
	message.endpoint = endpoint;
	message.response_callback = func;
	message.context = context;

	message.retry_timeout = retry_time_in_counts;

	message.encode = EncodePb;
	message.encodedata = &encodedata;

	message.sync = xSemaphoreCreateBinary();
	assert( message.sync );
	message.end = unblock_sync;
	message.response_handle = &response;

	DEBUG_PRINTF("NT sync %s",endpoint);

	assert( _asyncqueue );

	//add to queue
	if(xQueueSend( _asyncqueue, ( const void * )&message, portMAX_DELAY ) != pdTRUE)
	{
		LOGE("Cannot send request to _asyncqueue\n");
	    vSemaphoreDelete(message.sync);
		return false;
	}

    xSemaphoreTake(message.sync, portMAX_DELAY);
    vSemaphoreDelete(message.sync);

	return response.success;
}

static void NetworkTask_Thread(void * networkdata) {
	NetworkTaskServerSendMessage_t message;

	for (; ;) {

		if (!xQueueReceive( _asyncqueue, &message, portMAX_DELAY )) {
			vTaskDelay(1000);
			continue; //LOOP FOREVEREVEREVEREVER
		}
		if (message.begin) {
			message.begin(&message);
		}

		if( message.response_handle ) {
			*message.response_handle = nettask_send(&message);
		} else {
			nettask_send(&message);
		}

		if( message.end ) {
			message.end(&message);
		}
		vTaskDelay(100);
	}

}


int NetworkTask_AddMessageToQueue(const NetworkTaskServerSendMessage_t * message) {
    return xQueueSend( _asyncqueue, ( const void * ) message, 10 );
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
	// In this way the network task is encapsulated to its own module
	// no semaphore needs to expose to outside
	NetworkTaskData_t network_task_data;
	memset(&network_task_data, 0, sizeof(network_task_data));

	Init(&network_task_data);

	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(NetworkTask_Thread, "networkTask", stack_size, &network_task_data, 2, NULL);
}


