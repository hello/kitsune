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

#include "sl_sync_include_after_simplelink_header.h"
#include "crypto.h"
#include "hlo_proto_tools.h"
#include "streaming.pb.h"

int get_aes(uint8_t * dst);

void prep_for_pb(int type);

#define MAX_CHUNK_SIZE 512

static int id_counter = 0;

int frame_pipe_encode( pipe_ctx * pipe ) {
	uint8_t buf[MAX_CHUNK_SIZE];
	uint8_t hmac[SHA1_SIZE] = {0};
	int ret;
	int transfer_delay = 100;
	Preamble preamble_data;
	hlo_stream_t * hmac_payload_str = NULL;
	size_t size;

	uint8_t key[AES_BLOCKSIZE];
	get_aes(key);

	//make the preamble
	preamble_data.type = pipe->hlo_pb_type;
	preamble_data.has_auth = true;
	preamble_data.auth = Preamble_auth_type_HMAC_SHA1;
	preamble_data.has_id = true;
	preamble_data.id = ++id_counter;
	//encode it
	{
		pb_ostream_t preamble_ostream = pb_ostream_from_buffer(buf, sizeof(buf));
		pb_encode( &preamble_ostream, Preamble_fields, &preamble_data);
		size = preamble_ostream.bytes_written;
	}
	//send it
    size = htonl(size);
	ret = hlo_stream_transfer_all(INTO_STREAM, pipe->sink, (uint8_t*)&size,sizeof(size),transfer_delay);
	if(ret <= 0){
		goto enc_return;
	}
    size = ntohl(size);
	ret = hlo_stream_transfer_all(INTO_STREAM, pipe->sink, buf, size,transfer_delay);
	if(ret <= 0){
		goto enc_return;
	}

	DBG_FRAMEPIPE(" enc start %d\n", pipe->flush);

	//read out the size of the pb payload from the source stream
	ret = hlo_stream_transfer_until(FROM_STREAM, pipe->source, (uint8_t*)&size,sizeof(size), transfer_delay, &pipe->flush);
	if(ret <= 0){
		goto enc_return;
	}
	//and write it out to the sink
	ret = hlo_stream_transfer_all(INTO_STREAM, pipe->sink, (uint8_t*)&size, sizeof(size),transfer_delay);
	if(ret <= 0){
		goto enc_return;
	}
	size = ntohl(size);
	//now we can use the size to count down...

    //wrap the source in hmac stream
	hmac_payload_str = hlo_hmac_stream(pipe->sink, key, sizeof(key) );
	if(!hmac_payload_str){
		ret = HLO_STREAM_ERROR;
		goto enc_return;
	}
	//compute an hmac on each block until the end of the stream
	while( size != 0 ) {
		//full chunk unless we're on the last one
		size_t to_read = size > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : size;

		ret = hlo_stream_transfer_until(FROM_STREAM, pipe->source, buf,to_read, transfer_delay, &pipe->flush);
		DBG_FRAMEPIPE(" enc rd %d\n", ret);
		if(ret <= 0){
			goto enc_return;
		}
		ret = hlo_stream_transfer_until(INTO_STREAM, hmac_payload_str, buf,to_read, transfer_delay, &pipe->flush);
		DBG_FRAMEPIPE(" enc wr %d\n", ret);
		if(ret <= 0){
			goto enc_return;
		}

		// grab the running hmac and drop it in the stream
		get_hmac( hmac, hmac_payload_str );
		ret = hlo_stream_transfer_until(INTO_STREAM, pipe->sink, hmac, sizeof(hmac), transfer_delay, &pipe->flush);
		if(ret <= 0){
			goto enc_return;
		}
		//prevent underflow
		if( size < ret ) {
			LOGE("!enc underflow %d %d\n", size, ret);
			ret = HLO_STREAM_ERROR;
			goto enc_return;
		}
		size -= ret;
	}

	enc_return:
	if( hmac_payload_str ) {
		hlo_stream_close(hmac_payload_str);
	}
	DBG_FRAMEPIPE( " enc returning %d\n", ret );
	return ret;
}
int frame_pipe_decode( pipe_ctx * pipe ) {
	uint8_t buf[512];
	uint8_t hmac[2][SHA1_SIZE] = {0};
	int ret;
	int transfer_delay = 100;
	Preamble preamble_data;
	hlo_stream_t * hmac_payload_str = NULL;
	uint8_t key[AES_BLOCKSIZE];
	get_aes(key);

	//read out the pb preamble size
    uint32_t size;
	ret = hlo_stream_transfer_all( FROM_STREAM, pipe->source, (uint8_t*)&size,sizeof(size), transfer_delay );
	if(ret <= 0){
		goto dec_return;
	}
	size = ntohl(size);
	if(sizeof(buf) > size) {
		LOGE("header too big %d\n", size);
		ret = HLO_STREAM_ERROR;
		goto dec_return;
	}

	//read out the pb preamble and parse it
	ret = hlo_stream_transfer_all( FROM_STREAM, pipe->source, buf,size, transfer_delay );
	if(ret <= 0){
		goto dec_return;
	}
	{
		pb_istream_t preamble_istream = pb_istream_from_buffer(buf, size);
		if( !pb_decode( &preamble_istream, Preamble_fields, &preamble_data ) ) {
			DBG_FRAMEPIPE("    dec preamble fail\n" );
			return HLO_STREAM_ERROR;
		}
	}
	//notify the decoder what kind of pb is coming
	prep_for_pb(preamble_data.type);

	//read out the pb payload size
	ret = hlo_stream_transfer_all( FROM_STREAM, pipe->source, (uint8_t*)&size,sizeof(size), transfer_delay );
	if(ret <= 0){
		goto dec_return;
	}
	//write it for the pb parser
	ret = hlo_stream_transfer_all( INTO_STREAM, pipe->sink, (uint8_t*)&size,sizeof(size),transfer_delay);
	if(ret <= 0){
		goto dec_return;
	}
	size = ntohl(size);

	//wrap the source stream in an hmac stream and read the payload
	hmac_payload_str = hlo_hmac_stream(pipe->source, key, sizeof(key) );
	if(!hmac_payload_str){
		ret = HLO_STREAM_ERROR;
		goto dec_return;
	}

	//compute an hmac on each block until the end of the stream
	while( size != 0 ) {
		//full chunk unless we're on the last one
		size_t to_read = size > MAX_CHUNK_SIZE ? MAX_CHUNK_SIZE : size;

		ret = hlo_stream_transfer_all( FROM_STREAM, hmac_payload_str, buf,to_read, transfer_delay );
		if(ret <= 0){
			goto dec_return;
		}

		//read in and verify the HMAC
		ret = hlo_stream_transfer_all( FROM_STREAM, pipe->source, hmac[1],sizeof(hmac[1]), transfer_delay );
		if(ret <= 0){
			goto dec_return;
		}
		get_hmac( hmac[0], hmac_payload_str );
		if( memcmp(hmac[0], hmac[1], sizeof(hmac[1]))) {
			LOGE("    dec hmac mismatch\n");
			ret = HLO_STREAM_ERROR;
			goto dec_return;
		}
		if(ret <= 0){
			goto dec_return;
		}
		//dump the pb payload out to the parser
		//todo while chunks
		ret = hlo_stream_transfer_all(INTO_STREAM, pipe->sink, buf,to_read,transfer_delay);
		if(ret <= 0){
			goto dec_return;
		}
		//prevent underflow
		if( size < ret ) {
			LOGE("!dec underflow %d %d\n", size, ret);
			ret = HLO_STREAM_ERROR;
			goto dec_return;
		}
		size -= ret;
	}

	dec_return:
	if( hmac_payload_str ) {
		hlo_stream_close(hmac_payload_str);
	}
	DBG_FRAMEPIPE("    dec returning %d\n", ret );
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
