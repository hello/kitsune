/**
 * adds a Python Requests style API for HTTP stack
 */
#include "hlo_stream.h"
typedef struct{
	int code;
	int len;
	hlo_stream_t * content_stream; /** r/w stream **/
}hlo_http_response_t;

/**
 * primitive api for a direct socket stream
 */
hlo_stream_t * hlo_sock_stream(const char * host, uint8_t secure);
hlo_http_response_t hlo_http_get(const char * host);

void hlo_http_destroy(hlo_http_response_t * resp);
