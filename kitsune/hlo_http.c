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
//====================================================================
//Protected API Declaration
//
hlo_stream_t * hlo_http_get_opt(hlo_stream_t * sock, const char * host, const char * endpoint, const char * opt_extra_headers[]);
//====================================================================
//socket stream implementation
//
typedef struct{
	int sock;
}hlo_sock_ctx_t;
static unsigned long _get_ip(const char * host){
	unsigned long ip = 0;
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
static int _start_connection(unsigned long ip, security_type sec){
	int sock = -1;
	if( ip ){
		 sockaddr sAddr;
		 if(sec == SOCKET_SEC_SSL){
			 sAddr = _get_addr(ip, 443);
		//	 sock = sock
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

		 SlSockNonblocking_t enableOption;
		 enableOption.NonblockingEnabled = 1;//blocking mode
		 sl_SetSockOpt(sock,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption));

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
	if(!secure){
		int sock = _start_connection(_get_ip(host), SOCKET_SEC_NONE);
		if(sock >= 0){
			LOGI("sock is %d\r\n", sock);
			return hlo_stream_new(&impl, (void*)sock, HLO_STREAM_READ_WRITE);
		}
	}
	return NULL;
}

//====================================================================
//http requests impl
//
#include "tinyhttp/http.h"

#define SCRATCH_SIZE 1536
typedef struct{
	hlo_stream_t * sockstream;
	struct http_roundtripper rt;
	int code;
	int size;
	int active;
	char * content_itr;
	char scratch[SCRATCH_SIZE];
}hlo_http_context_t;

static void _response_header(void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue){
	hlo_http_context_t * session = (hlo_http_context_t*)opaque;
	//DISP("Header %s: %s\r\n", ckey, cvalue);
}
static void _response_code(void* opaque, int code){
	hlo_http_context_t * session = (hlo_http_context_t*)opaque;
	session->code = code;
	//DISP("Code %d\r\n", code);
}
static void _response_body(void* opaque, const char* data, int size){
	hlo_http_context_t * session = (hlo_http_context_t*)opaque;
	memcpy(session->content_itr, data, size);
	session->content_itr += size;
	session->size += size;
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
		LOGI("GET EOF %d bytes\r\n", session->size);
		return HLO_STREAM_EOF;
	}
	return content_size;
}
static int _close_get_session(void * ctx){
	hlo_http_context_t * session = (hlo_http_context_t*)ctx;
	http_free(&session->rt);
	hlo_stream_close(session->sockstream);
	vPortFree(session);
	return 0;
}
hlo_stream_t * hlo_http_get_opt(hlo_stream_t * sock, const char * host, const char * endpoint, const char * opt_extra_headers[]){
	hlo_stream_vftbl_t functions = (hlo_stream_vftbl_t){
		.write = NULL,
		.read = _get_content,
		.close = _close_get_session,
	};
	hlo_stream_t * ret = NULL;
	if (!sock ){
		goto no_sock;
	}

	hlo_http_context_t * session = pvPortMalloc(sizeof(*session));
	if( !session ){
		goto no_session;
	}else{
		memset(session, 0, sizeof(*session));
		session->active = 1;	/** always try to parse something **/
	}
	session->sockstream = sock;
	http_init(&session->rt,response_functions, session);
	ret = hlo_stream_new(&functions, session, HLO_STREAM_READ);
	if( !ret ) {
		goto no_stream;
	}
	strcat(session->scratch, "GET ");
	strcat(session->scratch, endpoint);
	strcat(session->scratch, " HTTP/1.1");
	strcat(session->scratch, "\r\n");
	strcat(session->scratch, "Host: ");
	strcat(session->scratch, host);
	strcat(session->scratch, "\r\n");
	strcat(session->scratch, "Accept: text/html, application/xhtml+xml, */*\r\n");
	const char ** itr = opt_extra_headers;
	while(itr){
		strcat(session->scratch, *itr);
		itr++;
		strcat(session->scratch, "\r\n");
	}
	strcat(session->scratch, "\r\n");
	int transfer_size = ustrlen(session->scratch);
	LOGI("%s", session->scratch);
	if( transfer_size == hlo_stream_transfer_all(INTO_STREAM, sock, (uint8_t*)session->scratch, transfer_size, 4)){
		return ret;
	}else{
		LOGE("GET Request Failed\r\n");
		hlo_stream_close(ret);
		return NULL;
	}
no_stream:
	vPortFree(session);
no_session:
no_sock:
	return ret;

}
