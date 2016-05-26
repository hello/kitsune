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
//signed stream impl
//
#include "hlo_pipe.h"
#include "crypto.h"
typedef struct{
	hlo_stream_t * base;
	SHA1_CTX sha;
	AES_CTX aes;
	int offset;						/** used only for verify stream to indicate the signature has been completed **/
									/** 0        16     26      48 **/
	uint8_t sig[AES_IV_SIZE + 32];	/** |  iv     | sha  | rand  | **/
}signed_stream_t;

static int _write_signed(void * ctx, const void * buf, size_t size){
	signed_stream_t * stream = (signed_stream_t*)ctx;
	int ret = hlo_stream_write(stream->base, buf, size);
	if( ret > 0 ){
		SHA1_Update(&stream->sha, buf, ret);
	}
	return ret;
}
static int _close_signed(void * ctx){
	signed_stream_t * stream = (signed_stream_t*)ctx;

	SHA1_Final(&stream->sig[AES_IV_SIZE], &stream->sha); /** offset the beginning due to aes iv **/
	AES_cbc_encrypt(&stream->aes, &stream->sig[AES_IV_SIZE], &stream->sig[AES_IV_SIZE], 32);
	int ret = hlo_stream_transfer_all(INTO_STREAM, stream->base, stream->sig, sizeof(stream->sig), 4);
	if( ret < 0 ){
		LOGE("Error writing signature %d\r\n", ret);
	}
	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
extern uint8_t aes_key[AES_BLOCKSIZE + 1];
hlo_stream_t * sign_stream(const hlo_stream_t * base){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_signed,
		.read = NULL,
		.close = _close_signed,
	};
	if( !base ) return NULL;
	signed_stream_t * ctx = pvPortMalloc(sizeof(*ctx));
	if( !ctx ){
		hlo_steam_close(base);
		return NULL;
	}

	ctx->base = base;
	SHA1_Init(&ctx->sha);
	AES_set_key(&ctx->aes, aes_key, ctx->aes.iv, AES_MODE_128); //todo real key
	hlo_stream_read(random_stream_open(), ctx->sig, sizeof(ctx->sig));

	return hlo_stream_new(&functions, ctx, HLO_STREAM_WRITE);
}
//====================================================================
//verify stream impl
//
hlo_stream_t * verify_stream(const hlo_stream_t * base){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
			.write = NULL,
			.read = _read_verified,
			.close = _close_verified,
	};
	if ( !base ) return NULL;
	signed_stream_t * ctx = pvPortMalloc(sizeof(*ctx));
	if ( !ctx ){
		hlo_steam_close(base);
		return NULL;
	}
}
