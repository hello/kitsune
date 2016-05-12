#include "hlo_http.h"

hlo_http_response_t * hlo_http_get(const char * host, const char * endpoint);

void hlo_http_destroy(hlo_http_response_t * resp);
