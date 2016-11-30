/**
 * adds a Python Requests style API for HTTP stack
 * Architecture:
 * --------------         ------------------                        ---------
 * |Sock Stream  | < - > |hlo_http_stream   |   -> response        | You     |
 * |             |       |with tiny http    |  <-> content_stream  |         |
 * --------------         ------------------                        ---------
 */
#include "hlo_stream.h"
#include "wifi_cmd.h"


/**
 * primitive sock stream
 * for the requsts API
 */
hlo_stream_t * hlo_sock_stream(const char * host, security_type secure);
hlo_stream_t * hlo_ws_stream( hlo_stream_t * base);

/**
 * The following is a higher level API that returns the body as a stream
 * returns a valid stream if successful, otherwise you may also inspect the @out_resp
 * field for the header
 *
 * Header is automatically filled out for you, for advanced mode, use the primitive
 * hlo_http_manual_* API instead
 */

hlo_stream_t * hlo_http_get(const char * url);
/**
 * post is a bidirectional stream
 * write first until the end of the message, then read the response.
 * half duplex, open -> write all -> read response until eof -> write all ...
 */
hlo_stream_t * hlo_http_post(const char * url, const char * content_type);

//this keeps the socket alive
int hlo_http_keep_alive(hlo_stream_t * post_stream, const char * host, const char * endpoint);
