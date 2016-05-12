/**
 * adds a Python Requests style API for HTTP stack
 */
#include "hlo_steam.h"
typedef struct{
	int code;
	int len;
	hlo_stream_t * content_stream; /** r/w stream **/
}hlo_http_response_t;


hlo_http_response_t * hlo_http_get(const char * host, const char * endpoint);

void hlo_http_destroy(hlo_http_response_t * resp);
