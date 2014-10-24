#include "networktask.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "wifi_cmd.h"
#include <simplelink.h>

#define NETWORK_TASK_QUEUE_DEPTH(10)
#define RECEIVE_BUFFER_SIZE (1024)

/*  TODO: pass */
static QueueHandle_t _queue = NULL;


static void Init(ServerInfo_t * info) {

	if (!info->queue) {
		info->queue = xQueueCreate(NETWORK_TASK_QUEUE_DEPTH,sizeof(NetworkTaskServerSendMessage_t));
	}

}


void NetworkTask_Thread(void * data) {
	uint8_t recv_buf[RECEIVE_BUFFER_SIZE]; //put this on the stack, we may yet one day have two of these tasks running
	NetworkTaskServerSendMessage_t message;
	NetworkResponse_t response;
	NetworkTaskData_t * taskdata = (const ServerInfo_t *)data;
	uint8_t * buf;;
	uint32_t buf_size;
	Init(taskdata);

	for (; ;) {

		if (!xQueueReceive( taskdata->queue, &message, portMAX_DELAY )) {
			continue; //LOOP FOREVEREVEREVEREVER
		}

		//figure out receive buffer situation
		if (message.decode_buf) {
			buf = message.decode_buf;
			buf_size = message.decode_buf_size;
		}
		else {
			buf = recv_buf;
			buf_size = RECEIVE_BUFFER_SIZE;
		}

		memset(&response,0,sizeof(response));

		response.success = true;

		//push to server
		if (send_data_pb_callback(taskdata->host,message.endpoint,buf,buf_size,message.encode) == 0) {

			//if we care about decoding, we decode using the decode callback
			if (message.decode) {
				if (decode_rx_data_pb_callback(buf,buf_size,message.decode) < 0) {
					response.success = false;
					response.flags |= NETWORK_RESPONSE_FLAG_FAILED_DECODE;
				}
			}
		}
		else {
			//failed to push, now what?
			response.success = false;
			response.flags |= NETWORK_RESPONSE_FLAG_NO_CONNECTION;

		}

		//let the requester know what happened
		if (message.response_callback) {
			message.response_callback(&response);
		}

	}

}
