#include "hlo_pipe.h"
#include "task.h"
#include "uart_logger.h"

static int pipe_transfer_step(hlo_pipe_t * pipe, uint8_t * buf){
	int ret = hlo_stream_transfer_all(FROM_STREAM, pipe->from,buf, pipe->buf_size, pipe->poll_delay);
	if(ret < 0){
		return ret;
	}
	return hlo_stream_transfer_all(INTO_STREAM, pipe->to, buf, pipe->buf_size, pipe->poll_delay);
}

hlo_pipe_t * hlo_pipe_new(hlo_stream_t * from, hlo_stream_t * to, uint32_t buf_size, uint32_t opt_poll_delay){
	hlo_pipe_t * ret = pvPortMalloc(sizeof(hlo_pipe_t));
	if(ret){
		ret->from = from;
		ret->to = to;
		ret->poll_delay = opt_poll_delay;
		ret->buf_size = buf_size;
	}
	return ret;
}


int hlo_pipe_run(hlo_pipe_t * pipe){
	if(!pipe || !pipe->from || !pipe->to){
		return HLO_STREAM_NO_IMPL;
	}
	uint8_t * buf = pvPortMalloc(pipe->buf_size);
	if(buf && pipe->from && pipe->to){
		int ret;
		while((ret =  pipe_transfer_step(pipe, buf)) >= 0){
			//do control stuff here
		}
		if(ret < 0){
			DISP("Stream ended due to %d\r\n", ret);
		}
		vPortFree(buf);
		return 0;
	}

	return HLO_STREAM_ERROR;
}

void hlo_pipe_destroy(hlo_pipe_t * pipe){
	//kbai
	hlo_stream_close(pipe->from);
	hlo_stream_close(pipe->to);
	vPortFree(pipe);
}

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

		if(ret < 0){
			return ret;
		}else{
			idx += ret;
			if(idx == buf_size){
				return buf_size;
			}
			vTaskDelay(transfer_delay);
		}
	}
	return buf_size;
}
