#include "hlo_proto_tools.h"
#include <stdlib.h>
#include "uart_logger.h"
#include "ble_cmd.h"
#include "ble_proto.h"

typedef struct{
	void * buf;
	size_t buf_size;
}buffer_desc_t;

static void encode_MorpheusCommand(hlo_future_t * result, void * context){
	MorpheusCommand * command = (MorpheusCommand*)context;
	pb_ostream_t stream = {0};
	uint8_t* heap_page;
	if(!command){
		LOGI("Inavlid parameter.\r\n");
		goto end;
	}
	command->version = PROTOBUF_VERSION;
	command->has_firmwareVersion = true;
	command->firmwareVersion = FIRMWARE_VERSION_INTERNAL;

	ble_proto_assign_encode_funcs(command);

	pb_encode(&stream, MorpheusCommand_fields, command);

	if(!stream.bytes_written){
		goto end;
	}

	heap_page = pvPortMalloc(stream.bytes_written);
	if(!heap_page){
		goto end;
	}
	memset(heap_page, 0, stream.bytes_written);

	stream = pb_ostream_from_buffer(heap_page, stream.bytes_written);

	if(pb_encode(&stream, MorpheusCommand_fields, command)){
		hlo_future_write(result,heap_page,stream.bytes_written, stream.bytes_written);
	}else{
		LOGI("encode protobuf failed: ");
		LOGI(PB_GET_ERROR(&stream));
		LOGI("\r\n");
	}
	vPortFree(heap_page);
end:
	return;
}

static void decode_MorpheusCommand(hlo_future_t * result, void * context){
	uint8_t * buf = ((buffer_desc_t*)context)->buf;
	size_t size =  ((buffer_desc_t*)context)->buf_size;
	MorpheusCommand command = {0};
    int err = 0;
    ///LOGI("proto arrv\n");
    ble_proto_assign_decode_funcs(&command);
    pb_istream_t stream = pb_istream_from_buffer(buf, size);
    bool status = pb_decode(&stream, MorpheusCommand_fields, &command);
    if(!status){
    	LOGI("Decoding protobuf failed, error: ");
    	LOGI(PB_GET_ERROR(&stream));
    	LOGI("\r\n");
    	err = -1;
    }else{
    	err = 0;
    }
    hlo_future_write(result, &command, sizeof(command),err); //WARNING shallow copy
    vPortFree(context);
}
static void decode_batched_periodic_data(hlo_future_t * result, void * context){
	uint8_t * buf = ((buffer_desc_t*)context)->buf;
	size_t size =  ((buffer_desc_t*)context)->buf_size;
	batched_periodic_data command = {0};
    int err = 0;
    ///LOGI("proto arrv\n");
    //batched_periodic_data_assign_decode_funcs(&command);
    pb_istream_t stream = pb_istream_from_buffer(buf, size);
    bool status = pb_decode(&stream, batched_periodic_data_fields, &command);
    if(!status){
    	LOGI("Decoding protobuf failed, error: ");
    	LOGI(PB_GET_ERROR(&stream));
    	LOGI("\r\n");
    	err = -1;
    }else{
    	err = 0;
    }
    hlo_future_write(result, &command, sizeof(command),err); //WARNING shallow copy
    vPortFree(context);
}
static void encode_batched_periodic_data(hlo_future_t * result, void * context){
	batched_periodic_data * data = (batched_periodic_data*)context;
	pb_ostream_t stream = {0};
	uint8_t* heap_page = NULL;
	int code = 0;
	pb_encode(&stream, batched_periodic_data_fields, data);
	if(!stream.bytes_written){
		code = -1;
	}else{
		heap_page = pvPortMalloc(stream.bytes_written);
		assert(heap_page);
		memset(heap_page, 0, stream.bytes_written);
		stream = pb_ostream_from_buffer(heap_page, stream.bytes_written);
		if(!pb_encode(&stream, batched_periodic_data_fields, data)){
			code = -2;
		}
	}
	hlo_future_write(result, heap_page, stream.bytes_written, code);
	vPortFree(heap_page);
	vPortFree(data);
}


hlo_future_t * MorpheusCommand_from_buffer(void * buf, size_t size){
	buffer_desc_t * desc = pvPortMalloc(sizeof(buffer_desc_t));
	if(desc){
		desc->buf = buf;
		desc->buf_size = size;
		return hlo_future_create_task_bg(
					decode_MorpheusCommand,
					desc,
					1024);
	}else{
		return NULL;
	}

}
hlo_future_t * batched_periodic_data_from_buffer(void * buf, size_t size){
	buffer_desc_t * desc = pvPortMalloc(sizeof(buffer_desc_t));
	if(desc){
		desc->buf = buf;
		desc->buf_size = size;
		return hlo_future_create_task_bg(
					decode_batched_periodic_data,
					desc,
					1024);
	}else{
		return NULL;
	}
}
hlo_future_t * buffer_from_MorpheusCommand(MorpheusCommand * src){
	return hlo_future_create_task_bg(encode_MorpheusCommand, src, 1536);
}
hlo_future_t * buffer_from_batched_periodic_data(batched_periodic_data * src){
	batched_periodic_data * copy = pvPortMalloc(sizeof(*copy));
	memcpy(copy, src, sizeof(*copy));
	return hlo_future_create_task_bg(encode_batched_periodic_data, copy, 1536);
}
