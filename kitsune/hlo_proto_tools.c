#include "hlo_proto_tools.h"
#include <stdlib.h>
#include "uart_logger.h"
#include "ble_cmd.h"
#include "ble_proto.h"

typedef struct{
	void * buf;
	size_t buf_size;
}buffer_desc_t;

static int encode_MorpheusCommand(void * result, size_t size, void * context){
	buffer_desc_t * desc = (buffer_desc_t*)result;
	MorpheusCommand * command = (MorpheusCommand *)context;
	pb_ostream_t stream = {0};
	uint8_t* heap_page;
	if(!command){
		LOGI("Inavlid parameter.\r\n");
		goto end;
	}
	memset(result, 0, size);
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
		desc->buf = heap_page;
		desc->buf_size = stream.bytes_written;
	}else{
		LOGI("encode protobuf failed: ");
		LOGI(PB_GET_ERROR(&stream));
		LOGI("\r\n");
		vPortFree(heap_page);
	}
end:
	return stream.bytes_written;

}

static int decode_MorpheusCommand(void * result, size_t buf_size, void * context){
	uint8_t * buf = ((buffer_desc_t*)context)->buf;
	size_t size =  ((buffer_desc_t*)context)->buf_size;
	MorpheusCommand command;
    memset(&command, 0, sizeof(command));
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
    }
    memcpy(result,&command,sizeof(MorpheusCommand));
    return err;
}


int MorpheusCommand_from_buffer(MorpheusCommand * dst, void * buf, size_t size){
	buffer_desc_t desc = (buffer_desc_t){
		.buf = buf,
		.buf_size = size,
	};
	if(0 <= hlo_future_read_once(
					hlo_future_create_task_bg(sizeof(MorpheusCommand), decode_MorpheusCommand, &desc, 4096),
					dst,
					sizeof(*dst))){
		return 0;
	}
	return -1;
}
void * buffer_from_MorpheusCommand(MorpheusCommand * src, int * out_size){
	buffer_desc_t desc = {0};
	hlo_future_read_once(
			hlo_future_create_task_bg(sizeof(desc), encode_MorpheusCommand, src, 4096),
			&desc,
			sizeof(desc));
	*out_size = desc.buf_size;
	return desc.buf;
}
