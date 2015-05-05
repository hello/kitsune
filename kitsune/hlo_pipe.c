#include "hlo_pipe.h"
#include "task.h"
#include "uart_logger.h"

static int pipe_transfer(hlo_pipe_t * pipe, uint8_t * buf){
	int read = 0;
	int idx = 0;
	while(read == 0){
		read = hlo_stream_read(pipe->from, buf, pipe->buf_size);
		if(read < 0){
				return read;
		}else if(read == 0){
			vTaskDelay((TickType_t)pipe->poll_delay);
		}
	}
	while(idx < read){
		int ret = hlo_stream_write(pipe->to, buf+idx, read - idx);
		if(ret < 0){
			return ret;
		}else{
			idx += ret;
			if(idx == read){
				break;
			}
			//TODO either poll on consumer or delay
			vTaskDelay((TickType_t)pipe->poll_delay);
		}
	}
	return 0;
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
