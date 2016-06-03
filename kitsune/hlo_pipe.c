#include "hlo_pipe.h"
#include "task.h"
#include "uart_logger.h"


int hlo_stream_transfer_until(transfer_direction direction,
							hlo_stream_t * stream,
							uint8_t * buf,
							uint32_t buf_size,
							uint32_t transfer_delay,
							bool * flush ) {

	int ret, idx = 0;
	while(idx < buf_size){
		if(direction == INTO_STREAM){
			ret = hlo_stream_write(stream, buf+idx, buf_size - idx);
		}else{
			ret = hlo_stream_read(stream, buf+idx, buf_size - idx);
		}
		if( ret > 0 ) {
			idx += ret;
		}
		if(( flush && *flush )||(ret == HLO_STREAM_EOF)){
			DISP("  END %d %d \n\n", idx, ret);
			if( idx ){
				return idx;
			}else{
				return ret;
			}
		}else if(ret < 0){
			return ret;
		}else{
			if(idx == buf_size){
				return idx;
			}
			if(ret == 0){
				vTaskDelay(transfer_delay);
			}
		}
	}
	return HLO_STREAM_ERROR;
}

int hlo_stream_transfer_all(transfer_direction direction,
							hlo_stream_t * stream,
							uint8_t * buf,
							uint32_t buf_size,
							uint32_t transfer_delay){
	return hlo_stream_transfer_until( direction, stream, buf, buf_size, transfer_delay, NULL);
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

#define DBG_FRAMEPIPE(...)

int frame_pipe_encode( pipe_ctx * pipe ) {
	uint8_t buf[512];
	int ret;
	int transfer_delay = 100;

	DBG_FRAMEPIPE(" enc start %d\n", pipe->flush);

	ret = hlo_stream_transfer_until(FROM_STREAM, pipe->source, buf,sizeof(buf), transfer_delay, &pipe->flush);
	DBG_FRAMEPIPE(" enc rd %d\n", ret);
	if(ret <= 0){
		DBG_FRAMEPIPE(" enc break %d\n", ret);
		return ret;
	}
	ret = hlo_stream_transfer_all(INTO_STREAM, pipe->sink, buf,ret,transfer_delay);
	pipe->state = ret;
	DBG_FRAMEPIPE( " enc returning %d\n", ret );
	return ret;
}
int frame_pipe_decode( pipe_ctx * pipe ) {
	uint8_t buf[512];
	int ret;
	int transfer_delay = 100;

	DBG_FRAMEPIPE("    dec len %d\n", short_len);
	ret = hlo_stream_read( pipe->source, buf,sizeof(buf) );
	DBG_FRAMEPIPE("    dec pipe read %d\n", ret);
	if(ret < 0){
		pipe->state = ret;
		DBG_FRAMEPIPE("    dec break %d\n", ret);
		return ret;
	}
	DBG_FRAMEPIPE("    dec read %d\n", ret);
	ret = hlo_stream_transfer_all(INTO_STREAM, pipe->sink, buf,ret,transfer_delay);
	DBG_FRAMEPIPE("    dec frame stop returning %d\n", ret );
	return ret;
 }
void thread_frame_pipe_encode(void* ctx) {
	pipe_ctx * pctx = (pipe_ctx*)ctx;
	while(frame_pipe_encode( pctx ) > 0) {
		vTaskDelay(100);
	}
	xSemaphoreGive(pctx->join_sem);
	vTaskDelete(NULL);
}
void thread_frame_pipe_decode(void* ctx) {
	pipe_ctx * pctx = (pipe_ctx*)ctx;
	while(frame_pipe_decode( pctx ) > 0) {
		vTaskDelay(100);
	}
	hlo_stream_end(pctx->sink);
	xSemaphoreGive(pctx->join_sem);
	vTaskDelete(NULL);
}
