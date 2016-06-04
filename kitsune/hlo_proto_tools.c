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
hlo_future_t * buffer_from_MorpheusCommand(MorpheusCommand * src){
	return hlo_future_create_task_bg(encode_MorpheusCommand, src, 1024);
}


//====================================================================
//hmac stream impl
//

#include "crypto.h"
typedef struct{
	hlo_stream_t * base;
	SHA1_CTX  ctx;
	uint8_t * key;
	size_t key_len;
}hmac_stream_t;

#define DBG_HMAC(...)

static int _write_hmac(void * ctx, const void * buf, size_t size){
	hmac_stream_t * stream = (hmac_stream_t*)ctx;
	int rv = 0;
	DBG_HMAC("hmac write\n");
	rv = hlo_stream_write(stream->base, (uint8_t*)buf, size); // passthrough
	DBG_HMAC("hmac wrote %d\n", rv);

	if( rv > 0 ) SHA1_Update(&stream->ctx, buf, rv);

	return rv;
}
static int _read_hmac(void * ctx, void * buf, size_t size){
	hmac_stream_t * stream = (hmac_stream_t*)ctx;
	int rv = 0;
	DBG_HMAC("hmac read\n");
	rv = hlo_stream_read(stream->base, buf, size); // passthrough
	DBG_HMAC("hmac red %d\n", rv);

	if( rv > 0 ) SHA1_Update(&stream->ctx, buf, rv);
	return rv;
}
static int _close_hmac(void * ctx){
	hmac_stream_t * stream = (hmac_stream_t*)ctx;
	DBG_HMAC("hmac close\n");
	//hlo_stream_close(stream->base); wrapper can be applied and removed from base stream without modifying it
	vPortFree(stream->key);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_hmac_stream( hlo_stream_t * base, uint8_t * key, size_t key_len ){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_hmac,
		.read = _read_hmac,
		.close = _close_hmac,
	};
	if( !base ) return NULL;
	DBG_HMAC("hmac open\n" );

	int i;
	hmac_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		goto hmac_open_fail;
	}
	memset(stream, 0, sizeof(*stream) );
	stream->base = base;
	SHA1_Init(&stream->ctx);

	stream->key = pvPortMalloc( key_len );
	memcpy( stream->key, key, key_len );
	stream->key_len = key_len;

	{
		uint8_t ipad[64] = {0};
		memcpy( ipad, stream->key, stream->key_len);
		for(i=0;i<sizeof(ipad);++i) {
			ipad[i] ^= 0x36;

			DBG_HMAC("%x:",ipad[i]);
		}DBG_HMAC("\n");
		SHA1_Update(&stream->ctx, ipad, sizeof(ipad));
	}
	DBG_HMAC("hmac ready\n" );

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
	hmac_open_fail:
	return NULL;
}
void get_hmac( uint8_t * hmac, hlo_stream_t * stream ) {
	hmac_stream_t * hmac_stream = (hmac_stream_t*)stream->ctx;
	uint8_t digest[SHA1_SIZE];
	SHA1_CTX  ctx = hmac_stream->ctx;
	int i;
	DBG_HMAC("hmac get\n" );

	SHA1_Final(digest, &ctx); //hmac = hash(i_key_pad | message) where | is concatenation

	for(i=0;i<SHA1_SIZE;++i) {
		DBG_HMAC("%x:",digest[i]);
	}DBG_HMAC("\n");


	uint8_t opad[64] = {0};
	memcpy( opad, hmac_stream->key, hmac_stream->key_len);
	for(i=0;i< sizeof(opad);++i) {
		opad[i] ^= 0x5c;
	}

	for(i=0;i< sizeof(opad);++i) {
		DBG_HMAC("%x:",opad[i]);
	}DBG_HMAC("\n");

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, opad, sizeof(opad));
	SHA1_Update(&ctx, digest, SHA1_SIZE);

	SHA1_Final(hmac, &ctx); // hmac = hash(o_key_pad | hash(i_key_pad | message))

	for(i=0;i<SHA1_SIZE;++i) {
		DBG_HMAC("%x:",hmac[i]);
	}DBG_HMAC("\n");

}



