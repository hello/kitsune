#include "hlo_http.h"
#include "portable.h"
#include "kit_assert.h"
#include "socket.h"
#include "simplelink.h"
#include "wifi_cmd.h"
#include "sl_sync_include_after_simplelink_header.h"
#include <strings.h>
#include "tinyhttp/http.h"

//---------
//socket stream implementation
//
typedef struct{
	int sock;
}hlo_sock_ctx_t;
static unsigned long _get_ip(const char * host){
	unsigned long ip = 0;
	if(0 == gethostbyname(host, strlen(host), &ip, SL_AF_INET)){
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
			 sAddr = _get_addr(ip, 443);
			 sock = socket(AF_INET, SOCK_STREAM, SL_IPPROTO_TCP);
		 }
		 if( sock < 0 ) goto exit;

		 timeval tv = (timeval){
			.tv_sec = 2,
			.tv_usec = 0,
		 };
		 sl_SetSockOpt(sock, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv) );
		 SlSockNonblocking_t enableOption;
		 enableOption.NonblockingEnabled = 0;//blocking mode
		 sl_SetSockOpt(sock,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption));

		 int retry = 5;
		 int rv;
		 do{
			 rv = connect(sock, &sAddr, sizeof(sAddr));
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
	}
	return rv;
}
static int _write_sock(void * ctx, const void * buf, size_t size){
	int sock = (int)ctx;
	uint8_t dud;
	int rv = recv(sock, dud, 0, 0);
	if( rv < 0 ){
		return rv;
	}
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
			return hlo_stream_new(&impl, (void*)sock, HLO_STREAM_READ_WRITE);
		}
	}
	return NULL;
}
