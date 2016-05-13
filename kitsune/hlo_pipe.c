#include "hlo_pipe.h"
#include "task.h"
#include "uart_logger.h"

int hlo_stream_transfer_all(transfer_direction direction,
							hlo_stream_t * stream,
							uint8_t * buf,
							uint32_t buf_size,
							uint32_t transfer_delay){
	int ret, idx = 0;
	while(idx < buf_size){
		if(direction == INTO_STREAM){
			ret = hlo_stream_write(stream, buf+idx, buf_size - idx);
		}else{
			ret = hlo_stream_read(stream, buf+idx, buf_size - idx);
		}

		if(ret == HLO_STREAM_EOF){
			goto transfer_done;
		}else if(ret < 0){
			return ret;
		}else{
			idx += ret;
			if(idx == buf_size){
				goto transfer_done;
			}
			if(ret == 0){
				vTaskDelay(transfer_delay);
			}
		}
	}
transfer_done:
	return idx;
}
int hlo_stream_transfer_between(
		hlo_stream_t * src,
		hlo_stream_t * dst,
		uint8_t * buf,
		uint32_t buf_size,
		uint32_t transfer_delay){

	int ret = hlo_stream_transfer_all(FROM_STREAM, src, buf,buf_size, transfer_delay);
	if(ret < 0){
		return ret;
	}
	return hlo_stream_transfer_all(INTO_STREAM, dst, buf,ret,transfer_delay);

}
