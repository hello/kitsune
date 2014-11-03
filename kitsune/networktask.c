#include "networktask.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "wifi_cmd.h"
#include <pb_encode.h>
#include "uartstdio.h"

#define NETWORK_TASK_QUEUE_DEPTH (10)
#define INITIAL_RETRY_PERIOD_COUNTS (1024)

static xQueueHandle _asyncqueue = NULL;
static xSemaphoreHandle _waiter = NULL;
static xSemaphoreHandle _syncmutex = NULL;

static NetworkResponse_t _syncsendresponse;

#define USE_DEBUG_PRINTF

#ifdef USE_DEBUG_PRINTF
#define DEBUG_PRINTF(...)  UARTprintf(__VA_ARGS__); UARTprintf("\r\n")
#else
static void nop(const char * foo,...) {  }
#define DEBUG_PRINTF(...) nop(__VA_ARGS__)
#endif

static void Init(NetworkTaskData_t * info) {

	DEBUG_PRINTF("NetTask::Init -- host is %s",info->host);


	if (!_asyncqueue) {
		_asyncqueue = xQueueCreate(NETWORK_TASK_QUEUE_DEPTH,sizeof(NetworkTaskServerSendMessage_t));
	}

	if (!_waiter) {
		_waiter = xSemaphoreCreateBinary();
	}

	if (!_syncmutex) {
		_syncmutex = xSemaphoreCreateMutex();
	}


}

static void SynchronousSendNetworkResponseCallback(const NetworkResponse_t * response) {
	memcpy(&_syncsendresponse,response,sizeof(NetworkResponse_t));

//	DEBUG_PRINTF("NetTask::SynchronousSendNetworkResponseCallback -- got callback");

	xSemaphoreGive(_waiter);


}

static uint32_t EncodePb(pb_ostream_t * stream, const void * data) {
	network_encode_data_t * encodedata = (network_encode_data_t *)data;
	uint32_t ret;

	ret = pb_encode(stream,encodedata->fields,encodedata->encodedata);

//	DEBUG_PRINTF("NetTask::EncodePb -- got callback");


	return ret;
}

int NetworkTask_SynchronousSendProtobuf(const char * endpoint, char* buf, uint32_t buf_size, const pb_field_t fields[], const void * structdata, int32_t retry_time_in_counts) {
	NetworkTaskServerSendMessage_t message;
	network_encode_data_t encodedata;
	int retcode = -1;

	memset(&message,0,sizeof(message));

	encodedata.fields = fields;
	encodedata.encodedata = structdata;

	//craft message
	message.endpoint = endpoint;
	message.response_callback = SynchronousSendNetworkResponseCallback;
	message.retry_timeout = retry_time_in_counts;

	message.encode = EncodePb;
	message.encodedata = &encodedata;

	message.decode_buf = (uint8_t *)buf;
	message.decode_buf_size = buf_size;

	DEBUG_PRINTF("NetTask::SynchronousSendProtobuf -- Request endpoint %s",endpoint);


	//critical section
	xSemaphoreTake(_syncmutex,portMAX_DELAY);

	memset(&_syncsendresponse,0,sizeof(_syncsendresponse));

	//add to queue
	xQueueSend( _asyncqueue, ( const void * )&message, portMAX_DELAY );

	//worry: what if callback happens before I get here?
	//wait until network callback happens
	xSemaphoreTake(_waiter,portMAX_DELAY);

	if (_syncsendresponse.success) {
		retcode = 0;
	}

	//end critical section
	xSemaphoreGive(_syncmutex);

	return retcode;

}

static uint32_t _encode_raw_protobuf(pb_ostream_t * stream, const void * encode_data)
{
    const array_data* array = (const array_data*)encode_data;
    if(pb_write(stream, array->buffer, array->length))
    {
        return array->length;
    }

    return 0;
}


int NetworkTask_SynchronousSendRawProtobuf(const char * endpoint, 
	const array_data* data_holder, 
	uint8_t * response_buf, uint32_t buf_size,
	int32_t retry_time_in_counts) 
{

	int retcode = -1;

	NetworkTaskServerSendMessage_t message;
	memset(&message,0,sizeof(message));

	//craft message
	message.endpoint = endpoint;
	message.response_callback = SynchronousSendNetworkResponseCallback;
	message.retry_timeout = retry_time_in_counts;

	message.encode = _encode_raw_protobuf;
	message.encodedata = data_holder;

	message.decode_buf = (uint8_t *)response_buf;
	message.decode_buf_size = buf_size;

	DEBUG_PRINTF("NetTask::SynchronousSendRawProtobuf -- Request endpoint %s", endpoint);


	//critical section
	xSemaphoreTake(_syncmutex,portMAX_DELAY);

	memset(&_syncsendresponse,0,sizeof(_syncsendresponse));

	//add to queue
	xQueueSend( _asyncqueue, ( const void * )&message, portMAX_DELAY );

	//worry: what if callback happens before I get here?
	//wait until network callback happens
	xSemaphoreTake(_waiter,portMAX_DELAY);

	if (_syncsendresponse.success) {
		retcode = 0;
	}

	//end critical section
	xSemaphoreGive(_syncmutex);

	return retcode;

}


int NetworkTask_AddMessageToQueue(const NetworkTaskServerSendMessage_t * message) {
    return xQueueSend( _asyncqueue, ( const void * ) message, 10 );
}


void NetworkTask_Thread(void * networkdata) {
	NetworkTaskServerSendMessage_t message;
	NetworkResponse_t response;
	NetworkTaskData_t * taskdata = (NetworkTaskData_t *)networkdata;
	uint8_t * buf;;
	uint32_t buf_size;
	int32_t timeout_counts;
	int32_t retry_period;
	uint32_t attempt_count;

	Init(taskdata);

	for (; ;) {

		if (!xQueueReceive( _asyncqueue, &message, portMAX_DELAY )) {
			vTaskDelay(1000);
			DEBUG_PRINTF("NetTask::Thread  -- looping");
			continue; //LOOP FOREVEREVEREVEREVER
		}


		memset(&response,0,sizeof(response));

		response.success = true;
		retry_period = INITIAL_RETRY_PERIOD_COUNTS;
		attempt_count = 0;
		timeout_counts = message.retry_timeout;

		while (1) {

			DEBUG_PRINTF("NetTask::Thread -- %s%s -- %d",taskdata->host,message.endpoint,attempt_count);

			/* prepare to prepare */
			if (message.prepare) {
				message.prepare(message.prepdata);
			}

			//push to server
			if (send_data_pb_callback(taskdata->host,
					message.endpoint,
					(char*)message.decode_buf,
					message.decode_buf_size,
					message.encodedata,
					message.encode) == 0) {

				//if we care about decoding via a protobuf callback, we use the callback
				if (message.decode) {
					if (decode_rx_data_pb_callback(buf,buf_size,message.decodedata,message.decode) < 0) {
						response.flags |= NETWORK_RESPONSE_FLAG_FAILED_DECODE;
					}
				}
			}
			else {
				//failed to push, now what?
				response.success = false;
				response.flags |= NETWORK_RESPONSE_FLAG_NO_CONNECTION;

			}

			/* unprepare */
			if (message.unprepare) {
				message.unprepare(message.prepdata);
			}


			//if failed response and there's some timeout time left, go delay and try again.
			if (!response.success && timeout_counts > 0) {

				if (timeout_counts < retry_period) {
					DEBUG_PRINTF("NetTask::Thread -- waiting %d ticks",retry_period);
					vTaskDelay(retry_period);
					timeout_counts = 0;
				}
				else {
					DEBUG_PRINTF("NetTask::Thread -- waiting %d ticks",retry_period);
					vTaskDelay(retry_period);
					timeout_counts -= retry_period;
					retry_period <<= 1;
				}
			}
			else {
				break;
			}

			attempt_count++;

		};

		if (response.success) {
			DEBUG_PRINTF("NetTask::Thread -- %s%s -- FINISHED (success)",taskdata->host,message.endpoint);
		}
		else {
			DEBUG_PRINTF("NetTask::Thread -- %s%s -- FINISHED (failure)",taskdata->host,message.endpoint);
		}



		//let the requester know we are done
		if (message.response_callback) {
			message.response_callback(&response);
		}




	}

}
