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
hlo_stream_t * hlo_http_post(const char * url, uint8_t sign, const char * content_type);

#include "pb.h"
int hlo_pb_encode( hlo_stream_t * stream,const pb_field_t * fields, void * structdata );
int hlo_pb_decode( hlo_stream_t * stream,const pb_field_t * fields, void * structdata );

void hlo_frame_stream_flush(hlo_stream_t * fs);
hlo_stream_t * hlo_frame_stream(size_t size);
