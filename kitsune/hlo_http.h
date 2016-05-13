/**
 * adds a Python Requests style API for HTTP stack
 * Architecture:
 * --------------         ------------------                        ---------
 * |Sock Stream  | < - > |hlo_http_stream   |   -> response        | You     |
 * |             |       |with tiny http    |  <-> content_stream  |         |
 * --------------         ------------------                        ---------
 */
#include "hlo_stream.h"


/**
 * primitive sock stream
 * for the requsts API
 */
hlo_stream_t * hlo_sock_stream(const char * host, uint8_t secure);

/**
 * The following is a higher level API that returns the body as a stream
 * returns a valid stream if successful, otherwise you may also inspect the @out_resp
 * field for the header
 *
 * Header is automatically filled out for you, for advanced mode, use the primitive
 * hlo_http_manual_* API instead
 */
typedef struct{
	int code;	/* 200 and such as */
	int len;	/* positive value for content length, 0 if chunked */
	int error;	/* any other internal error */
}hlo_http_response_t;

hlo_stream_t * hlo_http_get(const char * host, const char * endpoint, hlo_http_response_t * out_resp);
hlo_stream_t * hlo_http_post(const char * host,const char * endpoint, hlo_http_response_t * out_resp);
hlo_stream_t * hlo_https_get(const char * host, const char * endpoint, hlo_http_response_t * out_resp);
hlo_stream_t * hlo_https_post(const char * host,const char * endpoint, hlo_http_response_t * out_resp);

hlo_stream_t * hlo_http_manual_get(hlo_stream_t * sock, const char * method, hlo_http_response_t * out_resp);

int hlo_filter_http_get_body(hlo_stream_t * sock, hlo_stream_t * output, const char * method);
