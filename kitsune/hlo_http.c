#include "hlo_http.h"
#include "portable.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "wifi_cmd.h"
#include "sl_sync_include_after_simplelink_header.h"
#include <string.h>
#include "tinyhttp/http.h"
#include "hlo_pipe.h"
#include <string.h>
#include "bigint_impl.h"
#include "hlo_net_tools.h"
#include <ustdlib.h>
//====================================================================
//Protected API Declaration
//
hlo_stream_t * hlo_http_get_opt(hlo_stream_t * sock, const char * host, const char * endpoint);
hlo_stream_t * hlo_http_post_opt(hlo_stream_t * sock, const char * host, const char * endpoint, const char * content_type_str);
//====================================================================
//socket stream implementation
//
typedef struct{
	int sock;
}hlo_sock_ctx_t;
extern void set_backup_dns();
static unsigned long _get_ip(const char * host){
	unsigned long ip = 0;
	set_backup_dns();
	if(0 == gethostbyname((_i8*)host, strlen(host), &ip, SL_AF_INET)){
		LOGI("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
										 host, SL_IPV4_BYTE(ip, 3), SL_IPV4_BYTE(ip, 2),
									   SL_IPV4_BYTE(ip, 1), SL_IPV4_BYTE(ip, 0));
		return ip;
	}
	return 0;
}
static sockaddr _get_addr(unsigned long ip, uint16_t port){
	sockaddr sAddr;
	 sAddr.sa_family = AF_INET;
	 sAddr.sa_data[0] = (port>>8) & 0xFF;
	 sAddr.sa_data[1] = port & 0xFF;
	 sAddr.sa_data[2] = (char) ((ip >> 24) & 0xff);
	 sAddr.sa_data[3] = (char) ((ip >> 16) & 0xff);
	 sAddr.sa_data[4] = (char) ((ip >> 8) & 0xff);
	 sAddr.sa_data[5] = (char) (ip & 0xff);
	 return sAddr;
}
#define SL_SSL_CA_CERT_FILE_NAME "/cert/ca.der"
static int _start_connection(unsigned long ip, security_type sec){
	int sock = -1;
	if( ip ){
		 sockaddr sAddr;
		 if(sec == SOCKET_SEC_SSL){
			 sAddr = _get_addr(ip, 443);
			 sock = socket(AF_INET, SOCK_STREAM, SL_SEC_SOCKET);
			 if(sock <= 0){
				 goto exit;
			 }
			 unsigned char method = SL_SO_SEC_METHOD_TLSV1_2;
#ifdef USE_SHA2
			 unsigned int cipher = SL_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256;
#else
			 unsigned int cipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
#endif
			 if( sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECMETHOD, &method, sizeof(method) ) < 0 ||
			 				sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &cipher, sizeof(cipher)) < 0 ||
			 				sl_SetSockOpt(sock, SL_SOL_SOCKET, \
			 									   SL_SO_SECURE_FILES_CA_FILE_NAME, \
			 									   SL_SSL_CA_CERT_FILE_NAME, \
			 									   strlen(SL_SSL_CA_CERT_FILE_NAME))  < 0  ){
				 LOGI( "error setting ssl options\r\n" );
				 //TODO hook this to BLE
				 //ble_reply_wifi_status(wifi_connection_state_SSL_FAIL);
			 }
		 }else{
			 sAddr = _get_addr(ip, 80);
			 sock = socket(AF_INET, SOCK_STREAM, SL_IPPROTO_TCP);
		 }
		 if( sock < 0 ) goto exit;

		 timeval tv = (timeval){
			.tv_sec = 2,
			.tv_usec = 0,
		 };
		 sl_SetSockOpt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv) );

		 /*SlSockNonblocking_t enableOption;
		 enableOption.NonblockingEnabled = 1;//blocking mode
		 sl_SetSockOpt(sock,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption));*/

		 int retry = 5;
		 int rv;
		 do{
			 rv = connect(sock, &sAddr, sizeof(sAddr));
			 vTaskDelay(100);
		 }while(rv == SL_EALREADY && retry--);
		 if(rv < 0){
			 LOGI("Could not connect %d\n\r\n\r", rv);
			 sock = -1;
		 }
	}
exit:
	return sock;
}
static int _close_sock(void * ctx){
	int sock = (int)ctx;
	close(sock);
	return 0;
}
static int _read_sock(void * ctx, void * buf, size_t size){
	int sock = (int)ctx;
	int rv =  recv(sock, buf, size,0);
	if(rv == SL_EAGAIN){
		rv = 0;
	}else if (rv == 0){
		rv = HLO_STREAM_EOF;
	}
	return rv;
}
static int _write_sock(void * ctx, const void * buf, size_t size){
	int sock = (int)ctx;
	int rv;
	rv = send(sock, buf, size, 0);
	if( rv == SL_EAGAIN ){
		rv = 0;
	}
	return rv;
}
hlo_stream_t * hlo_sock_stream(const char * host, uint8_t secure){
	hlo_stream_vftbl_t impl = (hlo_stream_vftbl_t){
		.write = _write_sock,
		.read = _read_sock,
		.close = _close_sock,
	};
	int sock = -1;
	if(!secure){
		sock = _start_connection(_get_ip(host), SOCKET_SEC_NONE);
	}else{
		sock = _start_connection(_get_ip(host), SOCKET_SEC_SSL);
	}
	if(sock >= 0){
		LOGI("sock is %d\r\n", sock);
		return hlo_stream_new(&impl, (void*)sock, HLO_STREAM_READ_WRITE);
	}
	return NULL;
}

//====================================================================
//pb stream impl
#include "pb.h"

#define DBG_PBSTREAM(...)
#define PB_FRAME_SIZE 1024

typedef struct{
	hlo_stream_t * sockstream;	/** base socket stream **/
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
	state->stream_state = hlo_stream_transfer_all(INTO_STREAM, state->sockstream,(uint8_t*)inbuf, count, 4);

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

	state->stream_state = hlo_stream_transfer_all(FROM_STREAM, state->sockstream, inbuf, count, 4);

#ifdef DBGVERBOSE_PBSTREAM
	_dbg_pbstream_raw("IN", inbuf, count);
	DBG_PBSTREAM("RC: %d\t%d\n", transfer_size, state->stream_state );
#endif

	return transfer_size == state->stream_state;
}
int hlo_pb_encode( hlo_stream_t * stream, const pb_field_t * fields, void * structdata ){
	uint16_t short_count;
	hlo_pb_stream_context_t state;

	state.sockstream = stream;
	state.stream_state = 0;
    pb_ostream_t pb_ostream = { _write_pb_callback, (void*)&state, PB_FRAME_SIZE, 0 };

	bool success = true;

	/*pb_ostream_t sizestream = {0};
	pb_encode(&sizestream, fields, structdata); //TODO double check no stateful callbacks get hit here

	DBG_PBSTREAM("PB TX %d\n", sizestream.bytes_written);
	short_count = sizestream.bytes_written;
	int ret = hlo_stream_transfer_all(INTO_STREAM, stream, (uint8_t*)&short_count, sizeof(short_count), 4);

	success = success && sizeof(short_count) == ret; */
	success = success && pb_encode(&pb_ostream,fields,structdata);

	DBG_PBSTREAM("PBSS %d %d %d\n", state.stream_state, success, ret );
	if( state.stream_state > 0 ) {
		return success==true ? 0 : -1;
	}
	return state.stream_state;
}
int hlo_pb_decode( hlo_stream_t * stream, const pb_field_t * fields, void * structdata ){
	uint16_t short_count;
	hlo_pb_stream_context_t state;

	state.sockstream = stream;
	state.stream_state = 0;
	pb_istream_t pb_istream = { _read_pb_callback, (void*)&state, PB_FRAME_SIZE, 0 };

	bool success = true;

	int ret = hlo_stream_transfer_all(FROM_STREAM, stream, (uint8_t*)&short_count, sizeof(short_count), 4);

	success = success && sizeof(short_count) == ret;
	DBG_PBSTREAM("PB RX %d %d\n", ret, short_count);

	pb_istream.bytes_left = short_count;
	success = success && pb_decode(&pb_istream,fields,structdata);
	DBG_PBSTREAM("PBRS %d %d\n", state.stream_state, success );
	if( state.stream_state > 0 ) {
		return success==true ? 0 : -1;
	}
	return state.stream_state;
}

//====================================================================
//frame stream impl

typedef struct{
	uint8_t * buf;
	int32_t buf_pos;
	int32_t buf_size;
	bool eof;
	xSemaphoreHandle fill_sem, send_sem;
}hlo_frame_ctx_t;

#define DBG_FRAMESTREAM(...)

static int _close_frame(void * ctx){
	hlo_frame_ctx_t * fc = (hlo_frame_ctx_t*)ctx;
    DBG_FRAMESTREAM("Frame closed: %x\t%d\n", fc->buf, fc->eof);
	vPortFree(fc->buf);
	vPortFree(fc);
	return 0;
}
#define minval( a,b ) a < b ? a : b
static int _read_frame(void * ctx, void * buf, size_t size){
	hlo_frame_ctx_t * fc = (hlo_frame_ctx_t*)ctx;
	size_t to_copy = minval( size, fc->buf_pos );

    DBG_FRAMESTREAM("Frame RBEGIN: %x\t%d\n", fc->buf, size, fc->buf_pos );
	if( !fc->eof ) {
		//wait for a full frame or flush
		xSemaphoreTake(fc->send_sem, portMAX_DELAY);
	}

	//read out a frame
	memcpy(buf, fc->buf, to_copy );

	//shift any remaining bytes
	memcpy( fc->buf, fc->buf + to_copy, fc->buf_pos - to_copy );
	fc->buf_pos -= to_copy;

    DBG_FRAMESTREAM("Frame read: %x\t%d\n", fc->buf, to_copy);

	//unblock writers so they can fill a new frame
	xSemaphoreGive(fc->fill_sem );

	if( to_copy == 0 && fc->eof ) {
		return HLO_STREAM_EOF;
	}

	return to_copy;
}
static int _write_frame(void * ctx, const void * buf, size_t count){
	hlo_frame_ctx_t * fc = (hlo_frame_ctx_t*)ctx;
	uint8_t * inbuf = (uint8_t*)buf;
	int c = count;

	if ((fc->buf_pos + count ) >= fc->buf_size) {
		/* Will I exceed the buffer size? then send buffer */
		while ((fc->buf_pos + c) >= fc->buf_size) {
			//copy over
			memcpy(fc->buf + fc->buf_pos, inbuf,
					fc->buf_size - fc->buf_pos);

	        fc->buf_pos = fc->buf_size;
	        DBG_FRAMESTREAM("Frame: %x\t%d\n", fc->buf, fc->buf_pos);

			//block, our buffer is full
	        //warning, this block means we can't read and write from a framestream on the same thread without deadlock
	    	xSemaphoreGive(fc->send_sem );
	    	xSemaphoreTake(fc->fill_sem, portMAX_DELAY);

			c -= fc->buf_size - fc->buf_pos;
			inbuf += fc->buf_size - fc->buf_pos;
			fc->buf_pos = 0;
		}
		if( c > 0 ) {
			//copy to our buffer
			memcpy(fc->buf, inbuf, c);
			fc->buf_pos += c;
		}
	} else {
		//copy to our buffer
		memcpy(fc->buf + fc->buf_pos, inbuf, count);
		fc->buf_pos += count;
	}
    DBG_FRAMESTREAM("Frame write: %x\t%d\t%d\n", fc->buf, fc->buf_pos, count);
	return count;
}
// warning, must come only after all calls to write into the stream have returned, otherwise we may EOF with chunks leftover
void hlo_frame_stream_flush(hlo_stream_t * fs) {
	hlo_frame_ctx_t * fc = (hlo_frame_ctx_t*)fs->ctx;
    DBG_FRAMESTREAM("Flush: %x\t%d\n", fc->buf, fc->buf_size);
    fc->eof = true;
	xSemaphoreGive(fc->send_sem );
}
hlo_stream_t * hlo_frame_stream(size_t size) {
	hlo_stream_vftbl_t impl = (hlo_stream_vftbl_t){
		.write = _write_frame,
		.read = _read_frame,
		.close = _close_frame,
	};
	hlo_frame_ctx_t * fc = pvPortMalloc(sizeof(*fc));
	if(fc){
		fc->buf = pvPortMalloc(size);
		if(!fc->buf) {
			vPortFree(fc);
			return NULL;
		}
		fc->fill_sem = xSemaphoreCreateBinary();
		fc->send_sem = xSemaphoreCreateBinary();
		fc->buf_size = size;
		fc->buf_pos = 0;
		fc->eof = false;
	}else{
		return NULL;
	}
	return hlo_stream_new(&impl, (void*)fc, HLO_STREAM_READ_WRITE);
}

//====================================================================
//http requests impl
//Data Structures
#include "tinyhttp/http.h"

#define SCRATCH_SIZE 1536
typedef struct{
	hlo_stream_t * sockstream;	/** base socket stream **/
	struct http_roundtripper rt;/** tinyhttp context **/
	int code;					/** http response code, parsed by tinyhttp **/
	int len;					/** length of the response body tally **/
	int active;					/** indicates if the response is still active **/
	char * content_itr;			/** used by GET, itr to the outside buffer which we dump data do **/
	size_t scratch_offset;		/** used by POST, to collect data internally in bulk before sending **/
	char scratch[SCRATCH_SIZE]; /** scratch buffer, **MUST** be the last field **/
}hlo_http_context_t;
typedef enum{
	GET,
	POST
}http_method;
//====================================================================
//common functions
//
static void _response_header(void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue){
	//hlo_http_context_t * session = (hlo_http_context_t*)opaque;
	//DISP("Header %s: %s\r\n", ckey, cvalue);
}
static void _response_code(void* opaque, int code){
	hlo_http_context_t * session = (hlo_http_context_t*)opaque;
	session->code = code;
	//DISP("Code %d\r\n", code);
}
static void _response_body(void* opaque, const char* data, int size){
	hlo_http_context_t * session = (hlo_http_context_t*)opaque;
	if(session->content_itr){
		memcpy(session->content_itr, data, size);
		session->content_itr += size;
		session->len += size;
	}else{
		hlo_stream_write(uart_stream(), data, size);
	}

}
static void* _response_realloc(void* opaque, void* ptr, int size){
	return pvPortRealloc(ptr, size);
}
static const struct http_funcs response_functions = {
    _response_realloc,
    _response_body,
    _response_header,
    _response_code,
};
static hlo_stream_t * _new_stream(hlo_stream_t * sock , hlo_stream_vftbl_t * tbl, uint32_t stream_opts){
	hlo_stream_t * ret = NULL;
	if(!sock){
		return NULL;
	}
	hlo_http_context_t * session = pvPortMalloc(sizeof(*session));
	if(!session){
		return NULL;
	}else{
		memset(session, 0, sizeof(*session));
		session->active = 1;	/** always try to parse something **/
		session->sockstream = sock;
		http_init(&session->rt,response_functions, session);
	}
	ret =  hlo_stream_new(tbl, session, stream_opts);
	if( !ret ){
		vPortFree(session);
		return NULL;
	}
	return ret;
}
extern bool get_device_id(char * hex_device_id,uint32_t size_of_device_id_buffer);
extern const char * get_top_version(void);
#include "kitsune_version.h"
static int _write_header(hlo_stream_t * stream, http_method method, const char * host, const char * endpoint, const char * content_type){
	hlo_http_context_t * session = (hlo_http_context_t*)stream->ctx;
	char hex_device_id[DEVICE_ID_SZ * 2 + 1] = {0};
	if(!get_device_id(hex_device_id, sizeof(hex_device_id))){
		LOGE("get_device_id failed\n");
		return -1;
	}
	switch(method){
	default:
		return -1;
	case GET:
		usnprintf(session->scratch, sizeof(session->scratch),
					"GET %s HTTP/1.1\r\n"
		            "Host: %s\r\n"
		            "X-Hello-Sense-Id: %s\r\n"
		    		"X-Hello-Sense-MFW: %x\r\n"
		    		"X-Hello-Sense-TFW: %s\r\n"
		            "Accept: %s\r\n\r\n",
		            endpoint, host, hex_device_id, KIT_VER, get_top_version(), content_type);
		break;
	case POST:
		usnprintf(session->scratch, sizeof(session->scratch),
					"POST %s HTTP/1.1\r\n"
		            "Host: %s\r\n"
		            "Content-type: %s\r\n"
		            "X-Hello-Sense-Id: %s\r\n"
		    		"X-Hello-Sense-MFW: %x\r\n"
		    		"X-Hello-Sense-TFW: %s\r\n"
		            "Transfer-Encoding: chunked\r\n\r\n",
		            endpoint, host, content_type, hex_device_id, KIT_VER, get_top_version());
		break;
	}
	int transfer_size = ustrlen(session->scratch);
	LOGI("%s", session->scratch);
	if( transfer_size ==  hlo_stream_transfer_all(INTO_STREAM, session->sockstream, (uint8_t*)session->scratch, transfer_size, 4) ){
		return 0;
	}
	return -1;
}
//====================================================================
//Get
//
static int _get_content(void * ctx, void * buf, size_t size){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int bytes_to_process = min(size, SCRATCH_SIZE);
	session->content_itr = (char*)buf;
	const char * scratch_itr = session->scratch;
	int ndata = hlo_stream_read(session->sockstream, session->scratch, bytes_to_process);
	if( ndata > 0 ){
		while(session->active && ndata) {
			int processed;
			session->active = http_data(&session->rt, scratch_itr, ndata, &processed);
			ndata -= processed;
			scratch_itr += processed;
		}
	} else if(ndata < 0){
		return ndata;
	}

	int content_size = (session->content_itr - (char*)buf);
	if( http_iserror(&session->rt) ) {
		DISP("Has error\r\n");
		return HLO_STREAM_ERROR;
	} else if(content_size == 0 && !session->active){
		LOGI("GET EOF %d bytes\r\n", session->len);
		return HLO_STREAM_EOF;
	}
	return content_size;
}
static int _close_get_session(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int code = session->code;
	LOGI("GET returned code %d\r\n", session->code);
	http_free(&session->rt);
	hlo_stream_close(session->sockstream);
	vPortFree(session);
	return code;
}
//====================================================================
//Base implementation of get
//

hlo_stream_t * hlo_http_get_opt(hlo_stream_t * sock, const char * host, const char * endpoint){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = NULL,
		.read = _get_content,
		.close = _close_get_session,
	};
	hlo_stream_t * ret = _new_stream(sock, &functions, HLO_STREAM_READ);
	if( !ret ) {
		return NULL;
	}
	if( 0 == _write_header(ret, GET, host, endpoint, "*/*")){
		return ret;
	}else{
		LOGE("GET Request Failed\r\n");
		hlo_stream_close(ret);
		return NULL;
	}
}
//====================================================================
//User Friendly get
//
hlo_stream_t * hlo_http_get(const char * url){
	url_desc_t desc;
	if(0 == parse_url(&desc,url)){
		return hlo_http_get_opt(
				hlo_sock_stream(desc.host, (desc.protocol == HTTP)?0:1),
				desc.host,
				desc.path);
	}else{
		LOGE("Malformed URL %s\r\n", url);
	}
	return NULL;
}
//====================================================================
//Base implementation of post
//
//https://e2e.ti.com/support/wireless_connectivity/simplelink_wifi_cc31xx_cc32xx/f/968/t/407466
//nagle's algorithm is not supported on cc3200, so we roll our own.
#define CHUNKED_HEADER_SIZE (8+2) /*8 bytes of hex plus \r\n*/
#define CHUNKED_HEADER_FOOTER_SIZE (CHUNKED_HEADER_SIZE + 2) /* footer is also \r\n */
#define CHUNKED_BUFFER_SIZE  (SCRATCH_SIZE - CHUNKED_HEADER_FOOTER_SIZE) /* chunk + data + \r\n */
static int _post_chunked(hlo_http_context_t * session){
	itohexstring(session->scratch_offset, session->scratch);
	session->scratch[8] = '\r';
	session->scratch[9] = '\n';
	char * buffer_begin = session->scratch + CHUNKED_HEADER_SIZE;
	buffer_begin[session->scratch_offset] = '\r';
	buffer_begin[session->scratch_offset+1] = '\n';
	int transfer_len = session->scratch_offset + CHUNKED_HEADER_FOOTER_SIZE;
	return hlo_stream_transfer_all(INTO_STREAM, session->sockstream, (uint8_t*)session->scratch, transfer_len, 4);
}
static int _post_content(void * ctx, const void * buf, size_t size){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int bytes_rem = CHUNKED_BUFFER_SIZE - session->scratch_offset;
	int bytes_available = min(size, bytes_rem);
	if( bytes_available > 0){
		char * itr = session->scratch + CHUNKED_HEADER_SIZE + session->scratch_offset;
		memcpy(itr, buf, bytes_available);
		session->scratch_offset += bytes_available;
		return (int)bytes_available;
	}else if( session->scratch_offset ){
		if(_post_chunked(session) < 0){
			return HLO_STREAM_ERROR;
		} else {
			session->scratch_offset = 0;
			return _post_content(ctx, buf, size);
		}
	}
	return 0;
}
static int _finish_post(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int ret;
	static const char * const end_chunked = "0\r\n\r\n";
	int end_chunk_len = sizeof(end_chunked) - 1;
	if(session->scratch_offset){
		if((ret = _post_chunked(session))  < 0 ){
			return ret;
		}
	}
	if( end_chunk_len == hlo_stream_write(session->sockstream, end_chunked, end_chunk_len) ){
		return end_chunk_len;
	}
	return HLO_STREAM_ERROR;

}
static int _close_post_session(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int code = 0;
	if( _finish_post(ctx) >= 0 ){
		LOGI("\r\n=====\r\n");
		while(session->active){
			int ndata = hlo_stream_read(session->sockstream, session->scratch, sizeof(session->scratch));
			if( ndata < 0 ){
				goto cleanup;
			}
			const char * itr = session->scratch;
			while( session->active && ndata ){
				int read;
				session->active = http_data(&session->rt, itr, ndata, &read);
				ndata -= read;
				itr += read;
			}
		}//done parsing response
		LOGI("\r\n=====\r\n");
		if ( http_iserror(&session->rt) ){
			goto cleanup;
		}
	}
cleanup:
	if(session->code){
		code = session->code;
		LOGI("POST returned code %d\r\n", session->code);
	}else{
		code = 0;
		LOGE("POST Failed\r\n");
	}
	http_free(&session->rt);
	hlo_stream_close(session->sockstream);
	vPortFree(session);
	return code;
}
hlo_stream_t * hlo_http_post_opt(hlo_stream_t * sock, const char * host, const char * endpoint, const char * content_type_str){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _post_content,
		.read = NULL,
		.close = _close_post_session,
	};
	hlo_stream_t * ret = _new_stream(sock, &functions, HLO_STREAM_WRITE);
	if( !ret ){
		return NULL;
	}
	if( 0 == _write_header(ret, POST, host, endpoint, content_type_str) ){
		return ret;
	}else{
		LOGE("POST Request Failed\r\n");
		hlo_stream_close(ret);
		return NULL;
	}
}
//====================================================================
//User Friendly post
//
hlo_stream_t * hlo_http_post(const char * url, const char * content_type){
	url_desc_t desc;
	if(0 == parse_url(&desc,url)){
		const char * type = content_type ? content_type:"application/octet-stream";
		return hlo_http_post_opt(
				hlo_sock_stream(desc.host, (desc.protocol == HTTP)?0:1),
				desc.host,
				desc.path,
				type
			);
	}else{
		LOGE("Malformed URL %s\r\n", url);
	}
	return NULL;
}
