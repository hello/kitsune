#ifndef _NETWORKTASK_H_
#define _NETWORKTASK_H_

#include <pb.h>
#include "queue.h"

#define NETWORK_RESPONSE_FLAG_NO_CONNECTION (0x00000001)
#define NETWORK_RESPONSE_FLAG_FAILED_DECODE (0x00000002)

typedef struct {
	uint8_t success;
	uint32_t flags;
} NetworkResponse_t;

typedef uint32_t (*StreamSerializationCallback_t)(pb_ostream_t * stream);
typedef uint32_t (*StreamDeserializationCallback_t)(pb_istream_t * stream);

typedef void (*NetworkResponseCallback_t)(const NetworkResponse_t * response);


typedef struct {

	//optional encode and decode callbacks.  You can have one or both enabled.
	StreamSerializationCallback_t encode; //optional encode callback.  If you're just polling, you don't need this
	StreamDeserializationCallback_t decode; //optional decode callback.  If you're just sending, you don't need this

	const char * endpoint; //where on the server you wish to communicate to

	uint8_t * decode_buf; //by default we have 1K for you, but if you need more, put it here
	uint32_t decode_buf_size;

	NetworkResponseCallback_t response_callback;

} NetworkTaskServerSendMessage_t

typedef struct {
	const char * host;
	QueueHandle_t queue;

} NetworkTaskData_t;


#endif//_NETWORKTASK_H_
