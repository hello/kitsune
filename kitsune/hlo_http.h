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

hlo_stream_t * hlo_http_get(const char * url);
