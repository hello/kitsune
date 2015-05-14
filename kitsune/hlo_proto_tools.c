#include "hlo_proto_tools.h"
#include <stdlib.h>
#include "uart_logger.h"
#include "ble_cmd.h"

typedef struct{
	void * buf;
	size_t buf_size;
}buffer_desc_t;



void decode_MorpheusCommand(hlo_future_t * result, void * context){
	uint8_t * buf = ((buffer_desc_t*)context)->buf;
	size_t size =  ((buffer_desc_t*)context)->buf_size;
	MorpheusCommand command;
    memset(&command, 0, sizeof(command));
    int err = 0;
    ///LOGI("proto arrv\n");

    ble_proto_assign_decode_funcs(&command);

    pb_istream_t stream = pb_istream_from_buffer(buf, size);
    bool status = pb_decode(&stream, MorpheusCommand_fields, &command);
    ble_proto_remove_decode_funcs(&command);
    if(!status){
    	LOGI("Decoding protobuf failed, error: ");
    	LOGI(PB_GET_ERROR(&stream));
    	LOGI("\r\n");
    	err = -1;
    }
    hlo_future_write(result,&command,sizeof(command), err);
}


int MorpheusCommand_from_buffer(MorpheusCommand * dst, void * buf, size_t size){
	buffer_desc_t desc = (buffer_desc_t){
		.buf = buf,
		.buf_size = size,
	};
	int ret = -1;
	hlo_future_t * result = hlo_future_create_task(sizeof(MorpheusCommand), decode_MorpheusCommand, &desc);
	if(result){
		if(hlo_future_read(result,dst,sizeof(MorpheusCommand), portMAX_DELAY) >= 0){
			ret = 0;
		}
		hlo_future_destroy(result);
	}
	return ret;
}
