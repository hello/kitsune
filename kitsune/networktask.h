#ifndef _NETWORKTASK_H_
#define _NETWORKTASK_H_

#include <pb.h>
#include "ble_cmd.h"
#include "network_types.h"
#include "endpoints.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define NETWORK_RESPONSE_FLAG_NO_CONNECTION (0x00000001)

typedef struct {
	uint8_t success;
	uint32_t flags;
} NetworkResponse_t;

typedef void (*NetworkResponseCallback_t)(
		const NetworkResponse_t * response, char * reply_buf, int reply_sz,
		void * context);

typedef struct {
	/* This will be called before you encode or decode */
	network_prep_callback_t prepare;

	/* This will called AFTER you encode or decode, including during RETRIES  */
	network_prep_callback_t unprepare;
	void * prepdata;

	/* Called at the start of the session and after all retries, get context as arg */
	network_prep_callback_t begin, end;
	void * context;

	//protobuf
	const pb_field_t * fields;
	void * structdata;

	const char * host; //the server to which you wish to communicate
	const char * endpoint; //where on the server you wish to communicate to.  eg /audio/features

	int32_t retry_timeout;

	xSemaphoreHandle sync;
	NetworkResponseCallback_t response_callback;
	NetworkResponse_t * response_handle;
} NetworkTaskServerSendMessage_t;

typedef struct {
} NetworkTaskData_t;

void networktask_init(uint16_t stack_size);

/*
 * WARNING changing the host name here will only affect the Host
 * field in the HTTP headers, the socket it will talk on will
 * always connect to DATA_SERVER
 */
bool NetworkTask_SendProtobuf(bool blocking, const char * host,
		const char * endpoint, const pb_field_t fields[],
		void * structdata, int32_t retry_time_in_counts,
		NetworkResponseCallback_t func, void * context);
int NetworkTask_AddMessageToQueue(
		const NetworkTaskServerSendMessage_t * message);
int networktask_enter_critical_region();
int networktask_exit_critical_region();

#endif//_NETWORKTASK_H_
