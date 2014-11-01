#ifndef _NETWORKTASK_H_
#define _NETWORKTASK_H_

#include <pb.h>

#include "network_types.h"

#define NETWORK_RESPONSE_FLAG_NO_CONNECTION (0x00000001)
#define NETWORK_RESPONSE_FLAG_FAILED_DECODE (0x00000002)

typedef struct {
	uint8_t success;
	uint32_t flags;
} NetworkResponse_t;

typedef void (*NetworkResponseCallback_t)(const NetworkResponse_t * response);


typedef struct {
	/* This will be called before you encode or decode */
	network_prep_callback_t prepare;

	/* This will called AFTER you encode or decode, including during timeouts  */
	network_prep_callback_t unprepare;

	void * prepdata;




	//optional encode and decode callbacks.  You can have one or both enabled.
	network_encode_callback_t encode; //optional encode callback.  If you're just polling, you don't need this
	const void * encodedata; //optional extra data passed to your encode callback

	network_decode_callback_t decode; //optional decode callback.  If you're just sending, you don't need this
	void * decodedata; //optional extra data passed to your decode callback


	const char * endpoint; //where on the server you wish to communicate to

	uint8_t * decode_buf; //the buffer we dump our server response to
	uint32_t decode_buf_size;

	int32_t retry_timeout;

	NetworkResponseCallback_t response_callback;

} NetworkTaskServerSendMessage_t;

typedef struct {
	const char * host;
} NetworkTaskData_t;

void NetworkTask_Thread(void * networkdata);

int NetworkTask_SynchronousSendRawProtobuf(const char * endpoint, 
	const array_data* data_holder, 
	char * response_buf, uint32_t buf_size, 
	int32_t retry_time_in_counts);
int NetworkTask_SynchronousSendProtobuf(const char * endpoint, char * buf, uint32_t buf_size, const pb_field_t fields[], const void * structdata,int32_t retry_time_in_counts);
int NetworkTask_AddMessageToQueue(const NetworkTaskServerSendMessage_t * message);

#endif//_NETWORKTASK_H_
