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
	command->has_protocol_version = true;
	command->protocol_version = FIRMWARE_VERSION_INTERNAL;

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
//pb stream impl
#include "hlo_stream.h"
#include "hlo_pipe.h"
#include "sl_sync_include_after_simplelink_header.h"

#define DBG_PBSTREAM LOGI
#define PB_FRAME_SIZE 1024

typedef struct{
	hlo_stream_t * stream;	/** base socket stream **/
	int stream_state;
}hlo_pb_stream_context_t;

#ifdef DBGVERBOSE_PBSTREAM
static _dbg_pbstream_raw( char * dir, const uint8_t * inbuf, size_t count ) {
	int i;
	DBG_PBSTREAM("PB RAW %s\t%d\t%02x", dir, count, inbuf[0] );
	for( i=1; i<count; ++i) {
		DBG_PBSTREAM(":%02x",inbuf[i]);
		vTaskDelay(1);
	}
	DBG_PBSTREAM("\n");
}
#endif

// this will dribble out a few bytes at a time, may need to pipe it
static bool _write_pb_callback(pb_ostream_t *stream, const uint8_t * inbuf, size_t count) {
	int transfer_size = count;
	hlo_pb_stream_context_t * state = (hlo_pb_stream_context_t*)stream->state;
	state->stream_state = hlo_stream_transfer_all(INTO_STREAM, state->stream,(uint8_t*)inbuf, count, 200);

#ifdef DBGVERBOSE_PBSTREAM
	_dbg_pbstream_raw("OUT", inbuf, count);
	DBG_PBSTREAM("WC: %d\t%d\n", transfer_size, state->stream_state );
#endif
	return transfer_size == state->stream_state;
}
static bool _read_pb_callback(pb_istream_t *stream, uint8_t * inbuf, size_t count) {
	int transfer_size = count;
	hlo_pb_stream_context_t * state = (hlo_pb_stream_context_t*)stream->state;

#ifdef DBGVERBOSE_PBSTREAM
	DBG_PBSTREAM("PBREAD %x\t%d\n", inbuf, count );
#endif

	state->stream_state = hlo_stream_transfer_all(FROM_STREAM, state->stream, inbuf, count, 200);

#ifdef DBGVERBOSE_PBSTREAM
	_dbg_pbstream_raw("IN", inbuf, count);
	DBG_PBSTREAM("RC: %d\t%d\n", transfer_size, state->stream_state );
#endif

	return transfer_size == state->stream_state;
}
int hlo_pb_encode( hlo_stream_t * stream, const pb_field_t * fields, void * structdata ){
	uint32_t count;
	hlo_pb_stream_context_t state;

	state.stream = stream;
	state.stream_state = 0;
    pb_ostream_t pb_ostream = { _write_pb_callback, (void*)&state, PB_FRAME_SIZE, 0 };

	bool success = true;

	pb_ostream_t sizestream = {0};
	pb_encode(&sizestream, fields, structdata); //TODO double check no stateful callbacks get hit here

	DBG_PBSTREAM("PB TX %d\n", sizestream.bytes_written);
	count = sizestream.bytes_written;
	count = htonl(count);
	int ret = hlo_stream_transfer_all(INTO_STREAM, stream, (uint8_t*)&count, sizeof(count), 200);

	success = success && sizeof(count) == ret;
	success = success && pb_encode(&pb_ostream,fields,structdata);

	DBG_PBSTREAM("PBSS %d %d %d\n", state.stream_state, success, ret );
	if( state.stream_state > 0 ) {
		return success==true ? state.stream_state : HLO_STREAM_ERROR;
	} else {
		return HLO_STREAM_ERROR;
	}
}
int hlo_pb_decode( hlo_stream_t * stream, const pb_field_t * fields, void * structdata ){
	uint32_t count;
	hlo_pb_stream_context_t state;

	state.stream = stream;
	state.stream_state = 0;
	pb_istream_t pb_istream = { _read_pb_callback, (void*)&state, PB_FRAME_SIZE, 0 };

	bool success = true;

	int ret = hlo_stream_transfer_all(FROM_STREAM, stream, (uint8_t*)&count, sizeof(count), 200);

	success = success && sizeof(count) == ret;
	count = ntohl(count);
	DBG_PBSTREAM("PB RX %d %d\n", ret, count);

	pb_istream.bytes_left = count;
	success = success && pb_decode(&pb_istream,fields,structdata);
	DBG_PBSTREAM("PBRS %d %d\n", state.stream_state, success );
	if( state.stream_state > 0 ) {
		return success==true ? state.stream_state : HLO_STREAM_ERROR;
	} else {
		return HLO_STREAM_ERROR;
	}
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
int Cmd_testhmac(int argc, char *argv[]) {
	uint8_t hmac[SHA1_SIZE] = {0};
	hlo_stream_t * hmac_fifo = hlo_hmac_stream(fifo_stream_open( 100 ), (uint8_t*)argv[1], strlen(argv[1] ));

	hlo_stream_write( hmac_fifo, (uint8_t*)argv[2], strlen(argv[2] ));
	get_hmac( hmac, hmac_fifo );

	hlo_stream_close( hmac_fifo );
	return 0;
}

//====================================================================
//pb i/o impl
//
#include "hlo_pipe.h"
#include "hlo_http.h"
#include "async.pb.h"

#include "ack_set_entry.h"
#include "ack_set.h"
#define DBG_ACK(...)

#define MAX_PB_QUEUED 10
#define _MAX_RX_RECEIPTS 10

static hlo_stream_t * sock_stream = NULL;
static xSemaphoreHandle sock_stream_sem;
static xSemaphoreHandle ack_set_sem;
static xSemaphoreHandle new_outbound_msg_sem;
static ack_set outbound_msgs;

xQueueHandle hlo_ack_queue;

static void sock_checkup() {
	xSemaphoreTake(sock_stream_sem, portMAX_DELAY);
	if( sock_stream == NULL ) {
		sock_stream = hlo_ws_stream(hlo_sock_stream( "notreal", false ));
	}
	xSemaphoreGive(sock_stream_sem);
}
static void sock_close() {
	xSemaphoreTake(sock_stream_sem, portMAX_DELAY);
	hlo_stream_close(sock_stream);
	sock_stream = NULL;
	xSemaphoreGive(sock_stream_sem);
}

static void thread_out(void* ctx) {
	pipe_ctx p_ctx_enc;
	pb_msg * m;
	pb_msg ack;
	Ack ack_structdata;
	int num_acks_pending;
	uint64_t id;

	ack.fields = Ack_fields;
	ack.hlo_pb_type = Preamble_pb_type_ACK;
	ack.structdata = &ack_structdata;

	//todo while set is empty wait
	//todo while set is not empty iterate over it, calling the below funcs on each pb messenge
	while(1) {
		sock_checkup();

		if( xQueueReceive(hlo_ack_queue, &id, 0 ) ) {
			ack_structdata.has_message_id = true;
			ack_structdata.message_id = id;
			ack_structdata.has_status = true;
			ack_structdata.status = Ack_Status_SUCCESS; //todo realism

			m = &ack;
			DBG_ACK("sending ack\n", m->id);
		} else {
			//loop over all the messages in the outbound set
			xSemaphoreTake(ack_set_sem, portMAX_DELAY);
			m = ack_set_yield(&outbound_msgs);
			num_acks_pending = outbound_msgs.size;
			xSemaphoreGive(ack_set_sem);
			DBG_ACK("yielded\t%d\n", m->id);
		}

		//wait if empty
		if( m == NULL ) {
			xSemaphoreTake(new_outbound_msg_sem, 10000);
			continue;
		}

		if( sock_stream ) {
			hlo_stream_t * fifo_stream_out = fifo_stream_open( 64 );
			assert( fifo_stream_out );

			p_ctx_enc.source = fifo_stream_out;
			p_ctx_enc.sink = sock_stream;
			p_ctx_enc.flush = false;
			p_ctx_enc.join_sem = xSemaphoreCreateBinary();
			p_ctx_enc.state = 0;
			p_ctx_enc.hlo_pb_type = m->hlo_pb_type;
			p_ctx_enc.id = m->id;

			//bg pipe for sending out the data
			xTaskCreate(thread_frame_pipe_encode, "penc", 2*1024 / 4, &p_ctx_enc, 4, NULL);
			hlo_pb_encode( fifo_stream_out, m->fields, m->structdata );
			p_ctx_enc.flush = true; // this is safe, all the data has been piped
			xSemaphoreTake( p_ctx_enc.join_sem, portMAX_DELAY );
			vSemaphoreDelete( p_ctx_enc.join_sem );
			p_ctx_enc.flush = false;
			hlo_stream_close(fifo_stream_out);
			if(p_ctx_enc.state < 0 ) {
				DISP("enc state %d\n",p_ctx_enc.state );
				sock_close();
			}
		}
		//if we don't wait a bit between messages until we get an ack we flood the network with packets...
		//but if something new comes in we want to try and send it...
		//but if the server is struggling we want to back off...
		xSemaphoreTake(new_outbound_msg_sem, num_acks_pending*num_acks_pending*1000 + 1000);
	}
}
static void ack_rx( uint64_t acked_id ) {
	DBG_ACK("got ack\t%d\n", acked_id);

    xSemaphoreTake(ack_set_sem, portMAX_DELAY);
	ack_set_remove(&outbound_msgs, acked_id);
	xSemaphoreGive(ack_set_sem);
}

//DEMO IMPL
static void on_periodic_data( void * structdata ) {
	periodic_data * data = (periodic_data *)structdata;

	data->has_light = true;
	data->light = 10;
	LOGF("receieved %d\n", data->light );
}

#define SIZE_OF_LARGEST_PB 200
#define NUM_TYPES 2
static xSemaphoreHandle pb_sub_sem;
static xSemaphoreHandle pb_rx_complt_sem;
static int incoming_pb_type;
typedef struct {
	const pb_field_t * fields;
	void (*fmt_pb_data)(void *);
	void (*ondata)(void*);
} pb_subscriber;

static void on_ack_data( void * structdata)
{
	Ack * data = (Ack *)structdata;

	if( data->has_status ) DBG_ACK("ack sts %d\n", data->status);
	if( data->has_message_id ) ack_rx(data->message_id);
}
pb_subscriber subscriptions[NUM_TYPES] = {
		{ Ack_fields, NULL, on_ack_data},
		{ periodic_data_fields, NULL, on_periodic_data },
};

void hlo_prep_for_pb(int type) {
	xSemaphoreTake(pb_rx_complt_sem, portMAX_DELAY);
	incoming_pb_type = type;
	xSemaphoreGive(pb_sub_sem);
}

static void thread_in(void* ctx) {
	char * pb_data;
	pipe_ctx p_ctx_dec;

	pb_data = pvPortMalloc( SIZE_OF_LARGEST_PB ); //never freed

	while(1) {
		sock_checkup();

		if( sock_stream ) {
			hlo_stream_t * fifo_stream_in = fifo_stream_open( 768 );
			assert( fifo_stream_in );

			p_ctx_dec.source = sock_stream;
			p_ctx_dec.sink = fifo_stream_in;
			p_ctx_dec.flush = false;
			p_ctx_dec.join_sem = xSemaphoreCreateBinary();
			p_ctx_dec.state = 0;

			//bg pipe for receiving the data
			xTaskCreate(thread_frame_pipe_decode, "pdec", 2*1024 / 4, &p_ctx_dec, 4, NULL);

			xSemaphoreTake(pb_sub_sem, portMAX_DELAY);
			if( subscriptions[incoming_pb_type].fmt_pb_data ) {
				subscriptions[incoming_pb_type].fmt_pb_data(pb_data);
			}
			LOGF("\n\nR! %d\n\n",  hlo_pb_decode( fifo_stream_in, subscriptions[incoming_pb_type].fields, pb_data  ) );
			xSemaphoreTake( p_ctx_dec.join_sem, portMAX_DELAY );
			vSemaphoreDelete( p_ctx_dec.join_sem );
			hlo_stream_close(fifo_stream_in);
			if(p_ctx_dec.state < 0 ) {
				DISP("dec state %d\n",p_ctx_dec.state );
				sock_close();
			}
			if( subscriptions[incoming_pb_type].ondata ) {
				subscriptions[incoming_pb_type].ondata( pb_data );
			}
			xSemaphoreGive(pb_rx_complt_sem);
		}
	}
}
bool hlo_output_pb_wto( Preamble_pb_type hlo_pb_type, const pb_field_t * fields, void * structdata, TickType_t to ) {
	static uint64_t id = 0;

	//add the new pb msg to outbound set
	if( xSemaphoreTake(ack_set_sem, to) ) {
		pb_msg m = { fields, structdata, hlo_pb_type, ++id };
		DBG_ACK("sending\t%d\n", id);
		ack_set_update(&outbound_msgs, m, id);
		xSemaphoreGive(ack_set_sem);

		//signal tx thread it has work to do
		xSemaphoreGive(new_outbound_msg_sem);
		return true;
	}
	return false;
}
bool hlo_output_pb( Preamble_pb_type hlo_pb_type, const pb_field_t * fields, void * structdata ) {
	return hlo_output_pb_wto( hlo_pb_type, fields, structdata, portMAX_DELAY);
}

int Cmd_pbstr(int argc, char *argv[]) {
	sock_stream_sem = xSemaphoreCreateMutex();
	pb_sub_sem = xSemaphoreCreateBinary();
	pb_rx_complt_sem = xSemaphoreCreateBinary();
	xSemaphoreGive(pb_rx_complt_sem);

    hlo_ack_queue = xQueueCreate(_MAX_RX_RECEIPTS, sizeof(int64_t));
	ack_set_init(&outbound_msgs, MAX_PB_QUEUED);
	ack_set_sem = xSemaphoreCreateMutex();
	new_outbound_msg_sem = xSemaphoreCreateBinary();

	xTaskCreate(thread_out, "out", 2*1024 / 4, NULL, 4, NULL);
	xTaskCreate(thread_in, "in", 2*1024 / 4, NULL, 4, NULL);

	periodic_data data;

	data.has_light = true;
	data.light = 10;
	while(1) {
		LOGF("sending %d\n", data.light );
		hlo_output_pb( Preamble_pb_type_BATCHED_PERIODIC_DATA, periodic_data_fields, &data);
		vTaskDelay(5000);
	}

}
