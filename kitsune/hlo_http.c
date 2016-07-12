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
#include "hlo_proto_tools.h"
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

#define DBG_SOCKSTREAM(...)
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
			 sAddr = _get_addr(ip, 11000);
			 sock = socket(AF_INET, SOCK_STREAM, SL_IPPROTO_TCP);
		 }
		 if( sock < 0 ) goto exit;

		 timeval tv = (timeval){
			.tv_sec = 2,
			.tv_usec = 0,
		 };
		 sl_SetSockOpt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv) );

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
#if 0
		 SlSockNonblocking_t enableOption;
		 enableOption.NonblockingEnabled = 1;//blocking mode
		 sl_SetSockOpt(sock,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption));
#endif
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
	DBG_SOCKSTREAM("LISTENING %d\n", size);
	int rv =  recv(sock, buf, size,0);
    DBG_SOCKSTREAM("RECV %d\n", rv);

	if(rv == SL_EAGAIN){
		rv = 0;
	}else if (rv == 0){
		rv = HLO_STREAM_EOF;
	}
	return rv;
}
static int _write_sock(void * ctx, const void * buf, size_t size){
	int sock = (int)ctx;
	int rv = 0;

	DBG_SOCKSTREAM("SENDING %d\n", size);
	rv = send(sock, buf, size, 0);
	DBG_SOCKSTREAM("SENT %d\n", rv);
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
//websocket stream impl
//
#include "hlo_pipe.h"
#include "kitsune_version.h"

const char * get_top_version(void);

typedef struct{
	hlo_stream_t * base;
	int frame_bytes_read;
	int frame_bytes_towr;
}ws_stream_t;

static int _readstr_werr(hlo_stream_t * str, void * buf, size_t size) {
	int rv = hlo_stream_read( str, buf, size );
	if( rv <= 0 ) {
		return rv;
	}
	if( rv < size ) {
		return HLO_STREAM_EOF;
	}
	return rv;
}

static int _writestr_werr(hlo_stream_t * str, const void * buf, size_t size) {
	int rv = hlo_stream_write( str, buf, size );
	if( rv <= 0 ) {
		return rv;
	}
	if( rv < size ) {
		return HLO_STREAM_EOF;
	}
	return rv;
}
#define DBG_WS(...)
static int _write_ws(void * ctx, const void * buf, size_t size){
	ws_stream_t * stream = (ws_stream_t*)ctx;
	int rv = 0;

	DBG_WS("WS _write %d\n", size);
	/*0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+*/
	uint8_t wsh[8] = {0};

	DBG_WS("   WS owr %d %d\n", size, stream->frame_bytes_towr );

	wsh[0] |= 0x82; //final (only) frame of binary data...

	//cruft to match standard, not necessary as we use TLS
	wsh[1] |= 0x80; //set mask bit, but leave mask all 0...

	if( 0 == stream->frame_bytes_towr ) {
		stream->frame_bytes_towr = size;
		if( size < 126 ) {
			wsh[1] |= size;
			rv = _writestr_werr(stream->base, wsh, 6); //todo coalesce
			if(rv <= 0) return rv;
		} else if( size < 65536 ) {
			wsh[1] |= 126;
			*(uint16_t*)(wsh+2) = htons((unsigned short)size);
			rv = _writestr_werr(stream->base, wsh, 8);
			if(rv <= 0) return rv;
		} else {
			// won't send any frames bigger than 64k, don't need to worry about that case...
			LOGE("WS send %d too big\n", size);
		}
	}
	rv = hlo_stream_write(stream->base, (uint8_t*)buf, size);
	if( rv > 0 ) {
		stream->frame_bytes_towr -= rv;
	}
	DBG_WS("   WS fwr %d\n", stream->frame_bytes_towr );
	assert( stream->frame_bytes_towr >= 0 );
	return rv;
}
static int _read_ws(void * ctx, void * buf, size_t size){
	ws_stream_t * stream = (ws_stream_t*)ctx;
	int rv = 0;
	if( 0 == stream->frame_bytes_read ) {
		/*0                   1                   2                   3
	      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	     +-+-+-+-+-------+-+-------------+-------------------------------+
	     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
	     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
	     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
	     | |1|2|3|       |K|             |                               |
	     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
	     |     Extended payload length continued, if payload len == 127  |
	     + - - - - - - - - - - - - - - - +-------------------------------+*/
		uint8_t wsh[8];
		uint8_t opcode;
	    rv = _readstr_werr( stream->base, wsh, 2);
		if( rv <= 0 ) return rv;
		opcode = wsh[0] & 0xf;
		DBG_WS("WS frrd %x%x\n", wsh[0], wsh[1]);
		switch(opcode) {
		case 0x0: break;//continue
		case 0x2: break;//binary
		case 0x8:
			DBG_WS("WS closefr\n");
			return HLO_STREAM_EOF;
		case 0xa:
			DBG_WS("WS pong\n");
		case 0x9: //ping
			wsh[0] = 0x8A; //pong
			wsh[1] = 0;
			DBG_WS("WS ping\n");
			rv = _writestr_werr( stream->base, wsh, 2);
			if( rv <= 0 ) return rv;
			return 0;
		default:
			return HLO_STREAM_ERROR;
		}
		if( wsh[1] & 0x80 ) {
			LOGE("WS mask not supported!\n");
		}
		wsh[1] &= 0x7f;
		if( wsh[1] < 126 ) {
		    stream->frame_bytes_read = wsh[1];
		} else if( wsh[1] == 126 ) {
			rv = _readstr_werr( stream->base, wsh, 2);
			if( rv <= 0 ) return rv;

			stream->frame_bytes_read = (int)ntohs(*(unsigned short*)wsh);
		} else if( wsh[1] == 127 ) {
#if 1
			// won't send any frames bigger than 64k, don't need to worry about that case...
			LOGE("WS recv %d too big\n", size);
#else
			hlo_stream_transfer_all(FROM_STREAM, stream->base, wsh, 8, 200);
			stream->frame_bytes_read = ((uint64_t)ntohl((long*)wsh+4))<<32;
			stream->frame_bytes_read = ntohl((long*)wsh);
#endif
		}

		DBG_WS("WS frsz %d\n", stream->frame_bytes_read);
	}
	rv = hlo_stream_read(stream->base, buf, size);
	if( rv > 0 ) {
		stream->frame_bytes_read -= rv;
	}

	DBG_WS("WS rem %d\n", stream->frame_bytes_read);
	assert( stream->frame_bytes_read >= 0 );
	return rv;
}
static int _close_ws(void * ctx){
	ws_stream_t * stream = (ws_stream_t*)ctx;
	DBG_WS("WS close\n");
	hlo_stream_close(stream->base);
	vPortFree(stream);
	return 0;
}
hlo_stream_t * hlo_ws_stream( hlo_stream_t * base){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _write_ws,
		.read = _read_ws,
		.close = _close_ws,
	};
	if( !base ) return NULL;
	DBG_WS("WS open\n" );

	ws_stream_t * stream = pvPortMalloc(sizeof(*stream));
	if( !stream ){
		goto ws_open_fail;
	}
	memset(stream, 0, sizeof(*stream) );
	stream->base = base;

	{ //Websocket upgrade request
#define BUFSZ 512
#define DEV_STR_SZ (DEVICE_ID_SZ * 2 + 1)
		char * buf = pvPortMalloc(BUFSZ+DEV_STR_SZ);
	    char * hex_device_id = buf+BUFSZ;
	    int ret = 0;

	    assert(buf);

	    memset(buf, 0, BUFSZ );

	    if(!get_device_id(hex_device_id, DEV_STR_SZ))
	    {
	        LOGE("get_device_id failed\n");
	        goto ws_open_fail;
	    }
		usnprintf(buf, BUFSZ, "Get /dispatch HTTP/1.1\r\n"
				"Host: %s\r\n"
				"Upgrade: websocket\r\n"
				"Connection: Upgrade\r\n"
				"Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n" //cruft to match standard, not necessary as we use TLS
				"Sec-WebSocket-Protocol: echo\r\n"
				"Sec-WebSocket-Version: 13\r\n"
				"X-Hello-Sense-Id: %s\r\n"
				"X-Hello-Sense-MFW: %x\r\n"
				"X-Hello-Sense-TFW: %s\r\n"
				"\r\n",
				get_ws_server(), hex_device_id, KIT_VER, get_top_version());

		DBG_WS("sending\n%s", buf );
		hlo_stream_transfer_all(INTO_STREAM, stream->base, (uint8_t*)buf, strlen(buf), 200);
		memset(buf,0,BUFSZ);

		while( ret == 0 ) {
			vTaskDelay(200);
			ret =  hlo_stream_read(stream->base, buf, BUFSZ);
		}

		DBG_WS("rply\n%s", buf );
		if( ret < 0 ) {
			vPortFree(buf);
			goto ws_open_fail;
		}
		char * switching = strstr(buf, "HTTP/1.1 101");
		char * content = strstr(buf, "\r\n\r\n");
		if(!switching || !content) {
			vPortFree(buf);
			goto ws_open_fail;
		}
		int dlen = ret - ( content - buf ) - strlen("\r\n\r\n");

		DBG_WS("%s %s %d\n\n", switching, content, dlen );
		if( dlen > 1  ) {
			LOGE("%d extra data at start of ws!\n", dlen );
		}
		vPortFree(buf);
	}

	return hlo_stream_new(&functions, stream, HLO_STREAM_READ_WRITE);
ws_open_fail:
DBG_WS("WS FAIL\n" );

	hlo_stream_close(base);
	return NULL;
}
//====================================================================
//http requests impl
//Data Structures
#include "tinyhttp/http.h"

#define SCRATCH_SIZE 1536
typedef struct{
	hlo_stream_t * sockstream;	/** base socket stream **/
	struct http_roundtripper rt;/** tinyhttp context **/
	uint8_t * header_cache;		/** used to cache header **/
	int code;					/** http response code, parsed by tinyhttp **/
	int len;					/** length of the response body tally **/
	int response_active;					/** indicates if the response is still active **/
	enum{
		BEGIN_POST = 0,
		POSTING = 1,
		DONE_POST = 2,
	} post_state;				/** indicates if the post is finished **/
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
		//hlo_stream_write(uart_stream(), data, size);
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
		session->response_active = 1;	/** always try to parse something **/
		session->sockstream = sock;
		session->post_state = BEGIN_POST;
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
static int _generate_header(char * output, size_t output_size, http_method method, const char * host, const char * endpoint, const char * content_type ){
	char hex_device_id[DEVICE_ID_SZ * 2 + 1] = {0};
	if(!get_device_id(hex_device_id, sizeof(hex_device_id))){
		LOGE("get_device_id failed\n");
		return -1;
	}
	switch(method){
	default:
		return -1;
	case GET:
		usnprintf(output, output_size,
					"GET %s HTTP/1.1\r\n"
		            "Host: %s\r\n"
		            "X-Hello-Sense-Id: %s\r\n"
		    		"X-Hello-Sense-MFW: %x\r\n"
		    		"X-Hello-Sense-TFW: %s\r\n"
		            "Accept: %s\r\n\r\n",
		            endpoint, host, hex_device_id, KIT_VER, get_top_version(), content_type);
		break;
	case POST:
		usnprintf(output, output_size,
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
	return ustrlen(output);
}
//====================================================================
//Get
//
static int _get_content(void * ctx, void * buf, size_t size){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int bytes_to_process = min(size, SCRATCH_SIZE);
	session->content_itr = (char*)buf;
	const char * scratch_itr = session->scratch;
	if( !session->response_active ){
		return HLO_STREAM_EOF;
	}
	int ndata = hlo_stream_read(session->sockstream, session->scratch, bytes_to_process);
	if( ndata > 0 ){
		while(session->response_active && ndata) {
			int processed;
			session->response_active = http_data(&session->rt, scratch_itr, ndata, &processed);
			ndata -= processed;
			scratch_itr += processed;
		}
	} else if(ndata < 0){
		return ndata;
	}

	int content_size = (session->content_itr - (char*)buf);
	session->content_itr = NULL;	/* need to clean up here to prevent cached buffer tampering */
	if( http_iserror(&session->rt) ) {
		DISP("Has error\r\n");
		return HLO_STREAM_ERROR;
	} else if(content_size == 0 && !session->response_active){
		LOGI("GET EOF %d bytes\r\n", session->len);
		return HLO_STREAM_EOF;
	} else if(session->code != 0 && (session->code < 200 || session->code > 300)){
		LOGE("Response error, code %d\r\n", session->code);
		return HLO_STREAM_ERROR;
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
	hlo_http_context_t * session = (hlo_http_context_t*)ret->ctx;
	int transfer_len = _generate_header(session->scratch, sizeof(session->scratch), GET, host, endpoint, "*/*");
	DISP("%s", session->scratch);
	if(transfer_len > 0 &&  transfer_len == hlo_stream_transfer_all(INTO_STREAM, sock, session->scratch, transfer_len, 4) ){
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
	return hlo_stream_transfer_all(INTO_STREAM, session->sockstream, (uint8_t*)session->scratch, transfer_len, 200);
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
static int _post_content_with_header(void * ctx, const void * buf, size_t size){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int len;
	switch(session->post_state){
		case BEGIN_POST:
		case DONE_POST:
			//write header
			DISP("%s", session->header_cache);
			len = strlen((char*)session->header_cache);
			if( len != hlo_stream_transfer_all(INTO_STREAM, session->sockstream, (char*)session->header_cache, len, 4 ) ){
				return HLO_STREAM_ERROR;
			}
			session->post_state = POSTING;
			break;
		default:
			break;
	}
	return _post_content(ctx, buf, size);
}
static int _finish_post(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	static const char * const end_chunked = "0\r\n\r\n";
	int end_chunk_len = strlen(end_chunked);
	int ret = 0;
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
//this consumes the response until code has been retrieved, drops what's left of the body and returns
//should not be called in conjunction of read.
static int _fetch_response_code(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	while(session->response_active){
		int ndata = hlo_stream_read(session->sockstream, session->scratch, sizeof(session->scratch));
		if( ndata < 0 ){
			return HLO_STREAM_ERROR;
		}
		const char * itr = session->scratch;
		while( session->response_active && ndata ){
			int read;
			session->response_active = http_data(&session->rt, itr, ndata, &read);
			ndata -= read;
			itr += read;
		}
		if( http_iserror(&session->rt) ){
			return HLO_STREAM_ERROR;
		}
	}//done parsing response
	return 0;
}
static int _close_post_session(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int code = 0;
	if( session->post_state != DONE_POST ){		//if no response has been parsed by a read call, we just finish here
		session->post_state = DONE_POST;
		if( _finish_post(ctx) < 0 ){
			goto cleanup;
		}
		if( _fetch_response_code(ctx) < 0){
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
	if(session->header_cache){
		vPortFree(session->header_cache);
	}
	hlo_stream_close(session->sockstream);
	vPortFree(session);
	return code;
}
static int _get_post_response(void * ctx, void * buf, size_t size){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	int ret;
	switch(session->post_state){
	case BEGIN_POST:
		return HLO_STREAM_ERROR;		/** we may not get the response without posting header and content **/
	case POSTING:
		ret = _finish_post(ctx);
		if( ret < 0){
			return ret;
		}
		session->post_state = DONE_POST;
		session->response_active = 1;
		break;
	default:
	case DONE_POST:
		break;
	}
	return _get_content(ctx,buf,size);
}
hlo_stream_t * hlo_http_post_opt(hlo_stream_t * sock, const char * host, const char * endpoint, const char * content_type_str){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = _post_content_with_header,
		.read = _get_post_response,
		.close = _close_post_session,
	};
	hlo_stream_t * ret = _new_stream(sock, &functions, HLO_STREAM_READ_WRITE);
	if( !ret ){
		return NULL;
	}else{
		hlo_http_context_t * session = (hlo_http_context_t*)ret->ctx;
		int len = _generate_header(session->scratch, sizeof(session->scratch), POST, host, endpoint, content_type_str);
		if( len > 0 ){
			DISP("caching header\r\n");
			session->header_cache = pvPortMalloc(len + 1);
			assert(session->header_cache);
			ustrncpy((char*)session->header_cache, session->scratch, len+1);
		}else{
			hlo_stream_close(ret);
			return NULL;
		}
	}
	return ret;
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
