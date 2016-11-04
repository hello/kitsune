#include "hlo_pipe.h"
#include "task.h"
#include "uart_logger.h"
#include "sl_sync_include_after_simplelink_header.h"
#include "crypto.h"
#include "hlo_proto_tools.h"
#include "streaming.pb.h"

#define DBG_FRAMEPIPE(...)


int get_aes(uint8_t * dst);

#define KEY_AS_ID
#include "wifi_cmd.h"

#define MAX_CHUNK_SIZE 512

int frame_pipe_encode( pipe_ctx * pipe) {
	uint8_t buf[MAX_CHUNK_SIZE+1];
	hlo_stream_t * obufstr = fifo_stream_open( MAX_CHUNK_SIZE+1 );

	uint8_t hmac[SHA1_SIZE] = {0};
	int ret;
	int transfer_delay = 100;
	Preamble preamble_data = {0};
	hlo_stream_t * hmac_payload_str = NULL;
	size_t size;
#ifdef KEY_AS_ID
    char key[DEVICE_ID_SZ * 2 + 1] = {0};
    get_device_id(key, sizeof(key));
#else
	uint8_t key[AES_BLOCKSIZE];
	get_aes(key);
#endif

	//make the preamble
	preamble_data.type = pipe->hlo_pb_type;
	preamble_data.has_auth = true;
	preamble_data.auth = Preamble_auth_type_HMAC_SHA1;
	preamble_data.has_id = pipe->hlo_pb_type != Preamble_pb_type_ACK;
	if( preamble_data.has_id ) {
		preamble_data.id = pipe->id;
	}
	//encode it
	ret = hlo_pb_encode(obufstr, Preamble_fields, &preamble_data);
	if( ret != 0 ) {
		LOGE(" enc pbh parse fail %d\n", ret);
		goto enc_return;
	}
	DBG_FRAMEPIPE(" enc start %d\n", pipe->flush);

	//read out the size of the pb payload from the source stream
	//ignore flush (possible that all the data is already in the fifo and this will end the stream before we can read out the payload)
	ret = hlo_stream_transfer_all(FROM_STREAM, pipe->source, (uint8_t*)&size,sizeof(size), transfer_delay);
	if(ret <= 0){
		goto enc_return;
	}
	//and write it out to the sink
	ret = hlo_stream_transfer_all(INTO_STREAM, obufstr, (uint8_t*)&size, sizeof(size),transfer_delay);
	if(ret <= 0){
		goto enc_return;
	}
	size = ntohl(size);
	DBG_FRAMEPIPE(" enc payload size %d\n", size);
	//now we can use the size to count down...

    //wrap the source in hmac stream
	hmac_payload_str = hlo_hmac_stream(obufstr, key, sizeof(key) );
	if(!hmac_payload_str){
		ret = HLO_STREAM_ERROR;
		goto enc_return;
	}
	//compute an hmac on each block until the end of the stream
	while( size != 0 ) {
		//full chunk unless we're on the last one
		size_t to_read = size > (MAX_CHUNK_SIZE-SHA1_SIZE) ? (MAX_CHUNK_SIZE-SHA1_SIZE) : size;

		DBG_FRAMEPIPE(" enc chunk size %d\n", to_read);

		ret = hlo_stream_transfer_between_until(pipe->source,hmac_payload_str,buf,to_read,4, &pipe->flush);
		if(ret <= 0){
			goto enc_return;
		}

		DBG_FRAMEPIPE(" enc hmac\n" );

		// grab the running hmac and drop it in the stream
		get_hmac( hmac, hmac_payload_str );
		ret = hlo_stream_transfer_all(INTO_STREAM, obufstr, hmac, sizeof(hmac), transfer_delay);
		if(ret <= 0){
			goto enc_return;
		}

		DBG_FRAMEPIPE(" enc drop\n" );

		// drop the coalescing stream
		hlo_stream_end(obufstr);
		ret = hlo_stream_transfer_between(obufstr,pipe->sink,buf,sizeof(buf),4);
		if(ret <= 0){
			goto enc_return;
		}
		//prevent underflow
		if( size < to_read ) {
			LOGE("!enc underflow %d %d\n", size, to_read);
			ret = HLO_STREAM_ERROR;
			goto enc_return;
		}
		size -= to_read;
	}

	enc_return:
	hlo_stream_close(obufstr);
	if( hmac_payload_str ) {
		hlo_stream_close(hmac_payload_str);
	}
	DBG_FRAMEPIPE( " enc returning %d\n", ret );
	return ret;
}
extern xQueueHandle hlo_ack_queue;
int frame_pipe_decode( pipe_ctx * pipe ) {
	uint8_t buf[512];
	uint8_t hmac[2][SHA1_SIZE] = {0};
	int ret;
	size_t size;
	int transfer_delay = 100;
	Preamble preamble_data;
	hlo_stream_t * hmac_payload_str = NULL;
#ifdef KEY_AS_ID
    char key[DEVICE_ID_SZ * 2 + 1] = {0};
    !get_device_id(key, sizeof(key));
#else
	uint8_t key[AES_BLOCKSIZE];
	get_aes(key);
#endif
	//read out the pb preamble
	ret = hlo_pb_decode( pipe->source, Preamble_fields, &preamble_data );
	if( ret != 0 ) {
		LOGE("    dec pbh parse fail %d\n", ret);
		goto dec_return;
	}
	DBG_FRAMEPIPE("    dec type  %d\n", preamble_data.type );

	//notify the decoder what kind of pb is coming
	hlo_prep_for_pb(preamble_data.type);

	//read out the pb payload size
	ret = hlo_stream_transfer_all( FROM_STREAM, pipe->source, (uint8_t*)&size,sizeof(size), transfer_delay );
	if(ret <= 0){
		goto dec_return;
	}
	DBG_FRAMEPIPE("    dec payload size %d\n", ntohl(size) );
	//write it for the pb parser
	ret = hlo_stream_transfer_all( INTO_STREAM, pipe->sink, (uint8_t*)&size,sizeof(size),transfer_delay);
	size = ntohl(size);
	if(ret <= 0){
		goto dec_return;
	}
	DBG_FRAMEPIPE("    dec hmac begin\n" );

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
		DBG_FRAMEPIPE("    dec reading %d\n", to_read );

		ret = hlo_stream_transfer_all( FROM_STREAM, hmac_payload_str, buf,to_read, transfer_delay );
		if(ret <= 0){
			goto dec_return;
		}

		//read in and verify the HMAC
		ret = hlo_stream_transfer_all( FROM_STREAM, pipe->source, hmac[1],sizeof(hmac[1]), transfer_delay );
		if(ret <= 0){
			goto dec_return;
		}
		DBG_FRAMEPIPE("    dec cmpt hmac\n"  );

		get_hmac( hmac[0], hmac_payload_str );
		if( memcmp(hmac[0], hmac[1], sizeof(hmac[1]))) {
			LOGE("    dec hmac mismatch\n");
			ret = HLO_STREAM_ERROR;
			goto dec_return;
		}
		if(ret <= 0){
			goto dec_return;
		}

		DBG_FRAMEPIPE("    dec parsing\n"  );
		//dump the pb payload out to the parser
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
	if( preamble_data.has_id ){ //Ack that we got the message
		DBG_FRAMEPIPE("\t  ack %d sending\n", preamble_data.id);
		xQueueSend(hlo_ack_queue, &preamble_data.id, 0);
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

	frame_pipe_encode( pctx );
	xSemaphoreGive(pctx->join_sem);
	vTaskDelete(NULL);
}
void thread_frame_pipe_decode(void* ctx) {
	pipe_ctx * pctx = (pipe_ctx*)ctx;

	frame_pipe_decode( pctx );
	hlo_stream_end(pctx->sink);
	xSemaphoreGive(pctx->join_sem);
	vTaskDelete(NULL);
}
